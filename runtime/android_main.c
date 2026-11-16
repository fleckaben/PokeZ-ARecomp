// Android (SDL3's SDLActivity calls SDL_main in libmain.so): the folders the desktop builds take from the command line
// and the environment. Data (saves, settings, caches, the log) in the app's internal storage; the game code library
// next to this one in the APK's native library folder; the game files in internal storage if they are there, else
// in /storage/emulated/0/ZARecomp/extracted. Straight into the game: no start screen (it builds game code and can
// delete game files, neither of which an Android build can do).
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

int main(int argc, char **argv);

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
    setenv("LARECOMP_NO_LAUNCHER", "1", 1);
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
    char *args[] = {argv && argv[0] ? argv[0] : "zarecomp", root, "--play", NULL};
    return main(3, args);
}
#endif
