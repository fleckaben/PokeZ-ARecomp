#!/bin/bash
# Step 2: the runtime for Android arm64, as libmain.so (SDL3's SDLActivity loads it).
source "$(dirname "$0")/env.sh"
export PATH=$TB/bin:$PATH
LDF="-shared -fuse-ld=lld -Wl,--export-dynamic -Wl,-soname,libmain.so -L$DEPS/lib -lSDL3 -lavformat -lavcodec -lavutil -lswscale -lvulkan -lshaderc_shared -llog -landroid -ldl -lm -static-libstdc++"
make -j"$JOBS" ARCH=aarch64 B="$B" DEPS="$DEPS" CROSS="$CROSS" GENOPT="$GENOPT" \
     EXE="$B/libmain.so" LDFLAGS="$LDF" OSFLAGS="-Wno-format -DREX_ANDROID -DVK_USE_PLATFORM_ANDROID_KHR" "$B/libmain.so" "$@"
ls -la "$B/libmain.so"
