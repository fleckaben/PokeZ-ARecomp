# ZA Recomp on Android (AYN Odin 3) — beta

This is an unofficial fork of [ZA Recomp](https://github.com/Hexveil404/PokeZ-ARecomp) that runs on Android.
I only tested it on my **AYN Odin 3** (Snapdragon 8 Elite, Adreno 830 GPU).

## Who made this, honestly

I'm not a developer. I wanted to play on my Odin 3, so I did this port with an AI coding assistant (Claude, by
Anthropic). The assistant wrote the code and found the bugs. My part was testing on the console, reporting what looked
wrong and deciding what to fix. All the real work on the recompilation belongs to the authors of ZA Recomp. This
fork only adds the Android side.

**Status: beta.** The game starts and plays, but it has only been tested for a few hours. Expect bugs, and keep
backups of your saves.

## What was tested

I played from the start of the game until **just after the first Pokémon catches**. Nothing past that point has been
tested. Within that part:

- Boot, menus, the opening, the city, the first battles and catches
- The controller, and the F1 settings menu (Esc / Home, or Back + Start on the controller)
- Sleep and wake
- One mod loaded from `files/data/mods/<name>/romfs`
- The visual glitches below were fixed and checked in game: textures popping with the camera (arches, ivy, flower
  pots), a flashing white light on a rooftop, blotched eyes, a missing character and missing text on the Profile
  screen

## Performance (Odin 3, default settings)

- **30 fps** in the city
- About 1.2 CPU cores busy. The GPU runs around 37-45 % at 900 MHz.
- About 63-75 °C in play. Before the fix that keeps the game off the two fastest CPU cores, those cores reached
  100 °C and the fan ran at full speed.
- The F1 menu's higher shadow settings and ambient occlusion cost GPU time. Their cost was not measured.

## Known issues

- Profile screen (name entry before the game starts): going back with B and confirming the name again showed a
  garbage rectangle under the character. It has **not been retested** since the other fixes.
- Occasional single black frames
- Sound and cutscene videos not checked in depth
- Only the game's **2.0.2** update is supported (the Android memory setup uses 2.0.2's addresses)
- Only tested on the Odin 3. Other Snapdragon devices might work but are untested. Mali or PowerVR GPUs: no idea.
- This fork moves the game's memory to a lower address so it fits Android, so it **does not run on macOS**. For
  PC or Mac, use [the original project](https://github.com/Hexveil404/PokeZ-ARecomp).

## No game files, no APK

**No game files, keys or APK are provided, and none will be.** The APK you build contains code translated from
the game, so it is for your own device only: don't share or upload it. You need your own legally dumped copy of
Pokémon Legends: Z-A (with its 2.0.2 update) and your own Switch keys.

## Building (Linux or WSL on Windows)

You need the Android SDK and NDK (`ANDROID_SDK_ROOT`, default `~/Android/Sdk`, with build-tools and a platform
installed), a JDK, `cmake`, `ninja`, `clang`, `python3`, `curl` and `zip`.

1. **Game files.** Extract your game into `extracted/` as the main README's Linux section explains (`zaimport`,
   with your `prod.keys`, base game and 2.0.2 update).
2. **Translate the game code** for arm64: `make gen ARCH=aarch64`. This makes `generated-aarch64/`, which stays on
   your machine.
3. **Build**, from the repository's root folder:

   ```bash
   android/build_deps.sh       # SDL3, FFmpeg, shaderc for Android (once)
   android/build_runtime.sh    # libmain.so
   android/build_game.sh       # liblasgame.so (several minutes)
   android/build_apk.sh        # build/android/ZA-Recomp.apk
   ```

## Installing on the console

With the Odin connected over USB (USB debugging on):

```bash
adb install -r build/android/ZA-Recomp.apk
adb shell mkdir -p /sdcard/ZARecomp
adb push extracted /sdcard/ZARecomp/
adb shell appops set --uid local.za.recomp MANAGE_EXTERNAL_STORAGE allow
```

Then start **ZA Recomp** from the launcher. Saves, settings and the log (`lasrecomp.log`) are in the app's private
folder, `/data/data/local.za.recomp/files/data`. A `env.txt` file there (`KEY=VALUE` lines) sets options before the
game starts.

## What changed for Android (for developers)

- `runtime/android_main.c`: the entry point that SDL calls. It sets up folders, extracts fonts, keeps threads off the
  prime cores and starts the game directly, with no start screen.
- Guest memory moved from `0x70_0000_0000` to `0x10_0000_0000`: Android puts its libraries in the old range, and
  phones have a 39-bit address space.
- `hle/fiber.c`: an arm64 context switch, because Android's libc has no `swapcontext`.
- `nvn/vulkan/exec.c`: Adreno has no `VK_EXT_external_memory_host`. The game's physical memory is made of
  AHardwareBuffers mapped at their guest address and shown to Vulkan as one sparse buffer.
- `nvn/vulkan/exec.c`, redundant-state filter: Adreno forgets dynamic state and vertex/index buffer binds after a
  pipeline bind, so they are set again. This fixed textures popping with the camera (arches, ivy) and a flashing
  stretched light.
- Shader quad control on Adreno (fixes the eyes), D24S8 depth, unsupported Vulkan features turned off, identity
  pre-transform, swapchain kept on SUBOPTIMAL, the surface recreated after sleep, event polling that doesn't spin a
  core.

Every Android change is behind `#ifdef __ANDROID__`, except the guest memory move.

## Credits

- [ZA Recomp](https://github.com/Hexveil404/PokeZ-ARecomp) and its contributors: the whole recompilation and runtime
- SDL, FFmpeg, shaderc and the other libraries it uses
- The Android port: me, with Claude (Anthropic)

Pokémon is a trademark of Nintendo, Creatures Inc. and GAME FREAK inc. This project is not affiliated with or
endorsed by them.
