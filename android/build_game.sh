#!/bin/bash
# Step 3: the game code library (lasgame.so) from generated-aarch64/ (make gen ARCH=aarch64, from your own game files),
# as tools/za/build-linux.sh does: giant functions at -O0 first (clang -O1 can sit an hour on one), the rest through make.
source "$(dirname "$0")/env.sh"
export PATH=$TB/bin:$PATH
[ -d generated-aarch64/main ] || { echo "generated-aarch64/ is missing: run make gen ARCH=aarch64 first (see android/README.md)"; exit 1; }
MK=(ARCH=aarch64 B="$B" DEPS="$DEPS" CROSS="$CROSS" GENOPT="$GENOPT" EXE="$B/libmain.so")
FLAGS=$(make -s --eval='pv: ; @echo $(GENFLAGS)' "${MK[@]}" pv)
HOT=$(sed -n '/HOT_GIANTS\[\] = {/,/};/p' runtime/gamebuild.c | grep -o 'f_[0-9a-f]*' | sort -u)
giants=()
for f in generated-aarch64/main/*.c generated-aarch64/sdk/*.c; do
    name=$(awk '/^(__attribute__.*\) )?void f_.*\(Cpu \*c\) \{/ {n++; match($0, /f_[0-9a-f]+/); s=substr($0, RSTART, RLENGTH)}
        /^L_/ {l++} END {if (n == 1 && l > 1000) print s}' "$f")
    [ -n "$name" ] && ! grep -qx "$name" <<<"$HOT" && giants+=("$f")
done
echo "giant functions at -O0: ${#giants[@]}"
printf '%s\n' "${giants[@]}" | xargs -P "$JOBS" -I{} sh -c 'o="$1/obj/${2#generated-aarch64/}"; o="${o%.c}.o"; [ "$o" -nt "$2" ] ||
    { mkdir -p "$(dirname "$o")" && clang '"$CROSS"' $3 -O0 -c "$2" -o "$o"; }' _ "$B" {} "$FLAGS"
make -j"$JOBS" "${MK[@]}" GAMELIB_LDFLAGS="-shared -fuse-ld=lld -L$B -lmain" "$B/lasgame.so"
ls -la "$B/lasgame.so"
