#!/bin/bash
# Step 4: packages the APK by hand (no Gradle). The APK holds the game's translated code: it is for your own device
# only. Never share or upload it.
# Usage: android/build_apk.sh [out.apk]   (default: build/android/ZA-Recomp.apk)
source "$(dirname "$0")/env.sh"
set -e
SRC=$ZA/android
OUT=${1:-$B/ZA-Recomp.apk}
BT=$(ls -d "$SDK"/build-tools/* | sort -V | tail -1)
JAR=$(ls -d "$SDK"/platforms/android-*/android.jar | sort -V | tail -1)
WORK=$B/apk-work
STRIP=$TB/bin/llvm-strip
KEY=${ZA_KEYSTORE:-$SRC/debug.keystore}

[ -f "$B/libmain.so" ] && [ -f "$B/lasgame.so" ] || { echo "build_runtime.sh and build_game.sh first"; exit 1; }
[ -d "$DEPS/sdl-java" ] || { echo "build_deps.sh first (SDL's Java side)"; exit 1; }
if [ ! -f "$KEY" ]; then  # (a signing key of your own, made once, never committed)
    keytool -genkeypair -keystore "$KEY" -storepass android -keypass android -alias za -keyalg RSA -keysize 2048 \
            -validity 10000 -dname "CN=ZA Recomp local build"
fi

rm -rf "$WORK"
mkdir -p "$WORK"/classes "$WORK"/libs/lib/arm64-v8a "$WORK"/assets/res
cp third_party/fonts/BIZUDPGothic/BIZUDPGothic-Regular.ttf third_party/fonts/DejaVu/DejaVuSans.ttf \
   third_party/fonts/DejaVu/DejaVuSans-Bold.ttf third_party/fonts/Baloo2/Baloo2-SemiBold.ttf \
   tools/za/za-default.ini "$WORK"/assets/res/
cp docs/icon/icon_1024.png "$WORK"/assets/res/icon.png
L="$WORK"/libs/lib/arm64-v8a
$STRIP --strip-unneeded -o "$L/libmain.so" "$B/libmain.so"
$STRIP --strip-unneeded -o "$L/liblasgame.so" "$B/lasgame.so"
for lib in libSDL3.so libavutil.so libswscale.so libavcodec.so libavformat.so libshaderc_shared.so; do
  $STRIP --strip-unneeded -o "$L/$lib" "$(readlink -f "$DEPS/lib/$lib")"
done
# (FFmpeg's sonames carry the major version: Android loads them by that name)
for so in "$L"/*.so; do
  for need in $("$TB"/bin/llvm-readelf -d "$so" | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p'); do
    case "$need" in lib*.so.*) [ -f "$L/$need" ] || $STRIP --strip-unneeded -o "$L/$need" "$(readlink -f "$DEPS/lib/$need")";; esac
  done
done

javac -nowarn -Xlint:none -source 8 -target 8 -cp "$JAR" -d "$WORK"/classes \
  $(find -L "$DEPS/sdl-java" "$SRC/src" -name '*.java') 2>&1 | grep -v -E '^(warning|Note)' || true
"$BT"/d8 --release --min-api 33 --lib "$JAR" --output "$WORK" $(find "$WORK"/classes -name '*.class')

sed "s/android:versionCode=\"[0-9]*\"/android:versionCode=\"$(date +%s | cut -c1-9)\"/" "$SRC/AndroidManifest.xml" > "$WORK/AndroidManifest.xml"
"$BT"/aapt2 link --manifest "$WORK/AndroidManifest.xml" -I "$JAR" -A "$WORK"/assets --min-sdk-version 33 --target-sdk-version 33 -o "$WORK"/base.apk
(cd "$WORK" && zip -q -j base.apk classes.dex && cd libs && zip -q -r ../base.apk lib)
"$BT"/zipalign -f -p 4 "$WORK"/base.apk "$WORK"/aligned.apk
"$BT"/apksigner sign --ks "$KEY" --ks-pass pass:android --key-pass pass:android --out "$OUT" "$WORK"/aligned.apk
"$BT"/apksigner verify "$OUT"
ls -la "$OUT"
