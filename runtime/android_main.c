// Android (SDL3's SDLActivity calls SDL_main in libmain.so): the folders the desktop builds take from the command line
// and the environment. Data (saves, settings, caches, the log) in the app's internal storage; the game code library
// next to this one in the APK's native library folder; the game files in internal storage if they are there, else
// in /storage/emulated/0/ZARecomp/extracted. ZA Recomp's start screen first, or straight into the game
// (LARECOMP_ANDROID_PLAY, at the end).
#ifdef __ANDROID__
#define _GNU_SOURCE
#include <SDL3/SDL.h>
#include <dirent.h>
#include <sched.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <android/thermal.h>
#include "rt.h"
#include "settings.h"

int main(int argc, char **argv);

// ---- performance hints and the thermal step-down: adapted from MrSw4gGuy/PokeZ-Android (runtime/platform/android.c)

// Android's ADPF (API 33, looked up at run time): a session for the presenting thread gets the frame's real budget
// and each frame's work, so the scheduler keeps the CPU clocks that meet it instead of holding them up.
// LARECOMP_PERF_HINT=0: off.
typedef struct APerformanceHintManager APerformanceHintManager;
typedef struct APerformanceHintSession APerformanceHintSession;
void plat_perf_hint(uint64_t work_ns, uint64_t target_ns) {
    static int state = -1;  // (-1 not tried, 0 none, 1 on)
    static APerformanceHintSession *s;
    static int64_t cur_target;
    static int (*report)(APerformanceHintSession *, int64_t);
    static int (*update)(APerformanceHintSession *, int64_t);
    if (state < 0) {
        state = 0;
        const char *e = getenv("LARECOMP_PERF_HINT");
        if (e && !atoi(e)) return;
        void *h = dlopen("libandroid.so", RTLD_NOW);
        APerformanceHintManager *(*get)(void) = h ? (APerformanceHintManager * (*)(void)) dlsym(h, "APerformanceHint_getManager") : NULL;
        APerformanceHintSession *(*create)(APerformanceHintManager *, const int32_t *, size_t, int64_t) =
            h ? (APerformanceHintSession * (*)(APerformanceHintManager *, const int32_t *, size_t, int64_t))
                    dlsym(h, "APerformanceHint_createSession") : NULL;
        report = h ? (int (*)(APerformanceHintSession *, int64_t))dlsym(h, "APerformanceHint_reportActualWorkDuration") : NULL;
        update = h ? (int (*)(APerformanceHintSession *, int64_t))dlsym(h, "APerformanceHint_updateTargetWorkDuration") : NULL;
        APerformanceHintManager *m = get ? get() : NULL;
        int32_t tid = (int32_t)gettid();
        s = m && create && report && update ? create(m, &tid, 1, (int64_t)target_ns) : NULL;
        cur_target = (int64_t)target_ns;
        state = s != NULL;
        LOG("[perfhint] %s\n", s ? "performance hint session on" : "no performance hints here");
    }
    if (state != 1 || !work_ns) return;
    if ((int64_t)target_ns != cur_target) update(s, (int64_t)target_ns), cur_target = (int64_t)target_ns;
    report(s, (int64_t)work_ns);
}

// AThermal_getThermalHeadroom (API 31): the system's forecast of how close the device will be, over the next seconds,
// to its severe throttling point (1.0). Steps 100 / 85 / 70 / 60 %: down at 0.80 (10 s apart), back up below 0.60
// (30 s apart). The API wants a call a second at most. Only with the "Lower resolution when hot" setting.
static int thermal_step;
int plat_thermal_throttled(void) { return thermal_step > 0; }
double plat_thermal_scale(double now) {
    static const double steps[] = {1.0, 0.85, 0.7, 0.6};
    static AThermalManager *mgr;
    static int tried;
#define step thermal_step
    static double next_read, last_change = -1e9, last_line = -1e9;
    if (!g_settings.thermal_res) {
        if (step) LOG("[thermal] resolution by temperature: off\n");
        step = 0;
        return 1.0;
    }
    if (!tried) {
        tried = 1;
        mgr = AThermal_acquireManager();
        LOG("[thermal] resolution by temperature: %s\n", mgr ? "on" : "no thermal manager here");
    }
    if (!mgr || now < next_read) return steps[step];
    next_read = now + 1.0;
    float h = AThermal_getThermalHeadroom(mgr, 10);
    if (isnan(h)) return steps[step];
    int was = step;
    if (h >= 0.80f && step < 3 && now - last_change >= 10) step++;
    else if (h < 0.60f && step > 0 && now - last_change >= 30) step--;
    if (step != was) last_change = now;
    if (step != was || now - last_line >= 30) {
        last_line = now;
        LOG("[thermal] headroom %.2f: render scale %.0f%%%s\n", h, steps[step] * 100,
            step > was ? " - stepped down" : step < was ? " - stepped up" : "");
    }
    return steps[step];
}
#undef step

// The game's threads off the Snapdragon 8 Elite's two prime cores (4.32 GHz): the scheduler put the busiest ones
// there and those cores ran at 100 °C with the fan at full speed, while the game uses about 1.2 cores in all and the
// six 3.53 GHz cores stayed near 60 °C. Every thread of the process now, and the ones made later inherit it.
// LARECOMP_ANDROID_CPUS=<hex mask> (0xff: all).
static void keep_off_prime_cores(void) {
    const char *e = getenv("LARECOMP_ANDROID_CPUS");
    unsigned long mask = e ? strtoul(e, NULL, 16) : 0x3f;
    long n = sysconf(_SC_NPROCESSORS_CONF);
    if (!mask || n < 8) return;  // (not an 8-core part: as it was)
    cpu_set_t set;
    CPU_ZERO(&set);
    for (int i = 0; i < 64 && i < n; i++)
        if (mask >> i & 1) CPU_SET(i, &set);
    DIR *d = opendir("/proc/self/task");
    struct dirent *de;
    while (d && (de = readdir(d)))
        if (de->d_name[0] != '.') sched_setaffinity(atoi(de->d_name), sizeof set, &set);
    if (d) closedir(d);
}

__attribute__((visibility("default"))) int SDL_main(int argc, char **argv) {
    (void)argc;
    const char *internal = SDL_GetAndroidInternalStoragePath();
    static char data[1024], lib[1024], root[1024], probe[1100];
    snprintf(data, sizeof data, "%s/data", internal ? internal : "/data/local/tmp");
    mkdir(data, 0755);
    setenv("HOME", internal ? internal : data, 1);
    setenv("LARECOMP_DATA_HOME", data, 1);
    setenv("LARECOMP_DATA_ROOT", data, 1);
    // (no Android on-screen keyboard: the F1 menu asked for text input and it covered half the screen; the name entry
    // has its own keyboard, driven by the controller)
    SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");
    {   // data/env.txt: KEY=VALUE lines set before anything reads them (tests without a rebuild)
        char envf[1100], line[512];
        snprintf(envf, sizeof envf, "%s/env.txt", data);
        FILE *f = fopen(envf, "r");
        while (f && fgets(line, sizeof line, f)) {
            line[strcspn(line, "\r\n")] = 0;
            char *eq = strchr(line, '=');
            if (line[0] == '#' || !eq) continue;
            *eq = 0;
            setenv(line, eq + 1, 1);
        }
        if (f) fclose(f);
    }
    if (!getenv("LARECOMP_VK_PRESENT")) setenv("LARECOMP_VK_PRESENT", "fifo", 1);  // (mailbox: black frames on Adreno)
    keep_off_prime_cores();
    // (the whole guest window for shaders: the 256 MB default of integrated GPUs saves a desktop driver's userptr
    // checks, which the AHardwareBuffer window doesn't have - exec.c android_window)
    if (!getenv("LARECOMP_VK_GMEM_WINDOW")) setenv("LARECOMP_VK_GMEM_WINDOW", "0", 1);
    // the files the desktop builds keep next to the executable, from the APK's assets
    static const char *const res[] = {"BIZUDPGothic-Regular.ttf", "DejaVuSans.ttf", "DejaVuSans-Bold.ttf",
                                      "Baloo2-SemiBold.ttf", "za-default.ini", "icon.png"};
    static char resdir[1100], exe[1200];
    snprintf(resdir, sizeof resdir, "%s/res", data);
    mkdir(resdir, 0755);
    for (size_t i = 0; i < sizeof res / sizeof *res; i++) {
        char out[1300], in[64];
        snprintf(out, sizeof out, "%s/%s", resdir, res[i]);
        snprintf(in, sizeof in, "res/%s", res[i]);
        size_t len = 0;
        void *bytes = SDL_LoadFile(in, &len);
        if (!bytes) continue;
        SDL_SaveFile(out, bytes, len);
        SDL_free(bytes);
    }
    snprintf(exe, sizeof exe, "%s/zarecomp", resdir);
    setenv("LARECOMP_EXE_PATH", exe, 1);
    Dl_info di;
    if (dladdr((void *)SDL_main, &di) && di.dli_fname) {
        snprintf(lib, sizeof lib, "%s", di.dli_fname);
        char *slash = strrchr(lib, '/');
        if (slash) snprintf(slash + 1, sizeof lib - (size_t)(slash + 1 - lib), "liblasgame.so");
        setenv("LARECOMP_GAME_LIB", lib, 1);
    }
    snprintf(root, sizeof root, "%s/extracted", data);
    snprintf(probe, sizeof probe, "%s/exefs/main", root);
    if (access(probe, R_OK)) snprintf(root, sizeof root, "/storage/emulated/0/ZARecomp/extracted");
    // ZA Recomp's start screen (Play, Mods, Saves, settings) first, as on PC. LARECOMP_ANDROID_PLAY=1 in data/env.txt:
    // straight into the game instead (the start screen still comes up from "Title screen" in game)
    const char *pl = getenv("LARECOMP_ANDROID_PLAY");
    int play = pl && *pl && strcmp(pl, "0");
    char *args[] = {argv && argv[0] ? argv[0] : "zarecomp", root, play ? "--play" : NULL, NULL};
    return main(play ? 3 : 2, args);
}
#endif
