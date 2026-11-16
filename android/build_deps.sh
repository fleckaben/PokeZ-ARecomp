#!/bin/bash
# Step 1: the dependencies for Android arm64 (NDK), from the same pinned sources as tools/linux/build_deps.sh, into
# build/android-deps (ZA_ANDROID_DEPS). Each part is skipped when its output exists. Needs cmake, ninja, curl, python3.
source "$(dirname "$0")/env.sh"
set -euo pipefail
API=31
S=$ZA/build/linux/src
P=$DEPS
W=$ZA/build/android-deps-src
mkdir -p "$S" "$P/include" "$P/lib" "$W"
fetch() {
    [ -f "$S/$3" ] || { curl -fsSL --retry 8 --retry-delay 20 --retry-all-errors -o "$S/$3.part" "$1" && mv "$S/$3.part" "$S/$3"; }
    echo "$2  $S/$3" | sha256sum -c --quiet
}
unpack() { rm -rf "$W/$2" && mkdir -p "$W/$2" && tar -xf "$S/$1" -C "$W/$2" --strip-components 1; }
fetch https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz \
      7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68 SDL3-3.4.16.tar.gz
fetch https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz \
      8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e ffmpeg-9.0.2.tar.xz
fetch https://github.com/google/shaderc/archive/refs/tags/v2026.3.tar.gz \
      ee493ccf1b3038b4ef2fe024664c5eb2dc4bcc1f6b05b33e3909de0e19c81024 shaderc-2026.3.tar.gz
fetch https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.350.1.tar.gz \
      6d1bb65e49520344cc0a48af3dc02e993781efff14c7ebdcb8ae9fa23ddf7e83 Vulkan-Headers-1.4.350.1.tar.gz
fetch https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.3.0.tar.gz \
      c4f6bbe6b5a45c2eb610ca9d231158e313086d5b1a40c9922cb42b597419b14e VulkanMemoryAllocator-3.3.0.tar.gz
CM=(-G Ninja -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a
    -DANDROID_PLATFORM=android-$API -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$P -DCMAKE_INSTALL_LIBDIR=lib)
if [ ! -f "$P/include/vulkan/vulkan.h" ]; then
    unpack Vulkan-Headers-1.4.350.1.tar.gz vulkan-headers
    cp -R "$W/vulkan-headers/include/vulkan" "$W/vulkan-headers/include/vk_video" "$P/include/"
fi
if [ ! -f "$P/include/vma/vk_mem_alloc.h" ]; then
    unpack VulkanMemoryAllocator-3.3.0.tar.gz vma
    mkdir -p "$P/include/vma" && cp "$W/vma/include/vk_mem_alloc.h" "$P/include/vma/"
fi
if [ ! -f "$P/lib/libSDL3.so" ]; then
    unpack SDL3-3.4.16.tar.gz sdl
    cmake -S "$W/sdl" -B "$W/sdl/build" "${CM[@]}" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
    cmake --build "$W/sdl/build" -j$JOBS && cmake --install "$W/sdl/build"
    rm -rf "$P/sdl-java" && cp -r "$W/sdl/android-project/app/src/main/java" "$P/sdl-java"
fi
if [ ! -f "$P/lib/libavcodec.so" ]; then
    unpack ffmpeg-9.0.2.tar.xz ffmpeg
    TB=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin
    (cd "$W/ffmpeg" && ./configure --prefix="$P" --cc=$TB/aarch64-linux-android$API-clang --cxx=$TB/aarch64-linux-android$API-clang++ \
        --nm=$TB/llvm-nm --ar=$TB/llvm-ar --ranlib=$TB/llvm-ranlib --strip=$TB/llvm-strip --enable-cross-compile \
        --arch=aarch64 --target-os=android --enable-shared --disable-static --enable-pic \
        --disable-autodetect --disable-programs --disable-doc --disable-network --disable-avdevice --disable-avfilter \
        --disable-swresample --disable-everything \
        --enable-protocol=file --enable-demuxer=matroska,mov \
        --enable-decoder=vp9,vp8,vorbis,opus,h264,aac --enable-parser=vp9,vp8,vorbis,opus,h264,aac \
        > ../ffmpeg-configure.log && make -j$JOBS > ../ffmpeg-make.log 2>&1 && make install > /dev/null)
fi
if [ ! -f "$P/lib/libshaderc_shared.so" ]; then
    unpack shaderc-2026.3.tar.gz shaderc
    (cd "$W/shaderc" && python3 utils/git-sync-deps)
    cmake -S "$W/shaderc" -B "$W/shaderc/build" "${CM[@]}" -DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON \
        -DSHADERC_SKIP_COPYRIGHT_CHECK=ON -DSPIRV_SKIP_TESTS=ON -DSPIRV_SKIP_EXECUTABLES=ON -DENABLE_GLSLANG_BINARIES=OFF \
        -DSPIRV_TOOLS_BUILD_STATIC=ON -DBUILD_SHARED_LIBS=OFF -DANDROID_STL=c++_static
    cmake --build "$W/shaderc/build" --target shaderc_shared -j$JOBS
    mkdir -p "$P/include/shaderc"
    cp "$W/shaderc/libshaderc/include/shaderc/"*.h "$P/include/shaderc/"
    cp -P "$W/shaderc/build/libshaderc/libshaderc_shared.so"* "$P/lib/"
fi
echo "deps: $(cd "$P/lib" && ls *.so | tr '\n' ' ')"
