# Shared settings for the Android build scripts (sourced by them). Override any of these from the environment.
cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1
ZA=$PWD
SDK=${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}
NDK=${ANDROID_NDK_ROOT:-$(ls -d "$SDK"/ndk/* 2>/dev/null | sort | tail -1)}
[ -d "$NDK" ] || { echo "Android NDK not found (set ANDROID_SDK_ROOT or ANDROID_NDK_ROOT)"; exit 1; }
TB=$NDK/toolchains/llvm/prebuilt/linux-x86_64
DEPS=${ZA_ANDROID_DEPS:-$ZA/build/android-deps}
B=$ZA/build/android
JOBS=${JOBS:-$(nproc)}
API=33
CROSS="--target=aarch64-linux-android$API --sysroot=$TB/sysroot -fPIC"
GENOPT="-O1 -DGUEST_VOLATILE"
