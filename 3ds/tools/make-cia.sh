#!/bin/sh
# Builds zatackax.cia from the build in 3ds/ (after `make`): the program, the
# banner of banner.png and the settings of app.rsf. Needs makerom and bannertool
# in the PATH, and python3 (for the silence under the banner).
set -e
cd "$(dirname "$0")/.."

for tool in makerom bannertool python3; do
    command -v "$tool" >/dev/null 2>&1 || { echo "make-cia.sh: $tool not found" >&2; exit 1; }
done
for file in zatackax.elf zatackax.smdh banner.png app.rsf romfs-build; do
    [ -e "$file" ] || { echo "make-cia.sh: $file is missing, run make first" >&2; exit 1; }
done

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# A banner has a sound; this one is three seconds of silence.
python3 - "$work/silence.wav" <<'PY'
import sys, wave
w = wave.open(sys.argv[1], "wb")
w.setnchannels(1)
w.setsampwidth(2)
w.setframerate(22050)
w.writeframes(b"\0\0" * 22050 * 3)
w.close()
PY

bannertool makebanner -i banner.png -a "$work/silence.wav" -o "$work/banner.bnr"
makerom -f cia -o zatackax.cia -elf zatackax.elf -rsf app.rsf \
    -icon zatackax.smdh -banner "$work/banner.bnr" -exefslogo -target t
echo "made $(pwd)/zatackax.cia"
