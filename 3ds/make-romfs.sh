#!/bin/sh
# Assemble the romfs of the 3DS build in romfs-build/.
#
# The game data comes from ../data. The sounds are converted to 22.05 kHz mono
# Ogg Vorbis: the originals (44.1 kHz stereo, 2.8 MB for the music alone) take
# the 3DS's CPU too long to decode and make the game stutter and load slowly.
# The converted files are not part of the repository.
set -e
cd "$(dirname "$0")"

out=romfs-build
mkdir -p "$out"
cp -ru ../data/. "$out"/
# 3DS specific files (the pixel font): assets/ is laid over the game's data.
cp -ru assets/. "$out"/

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "make-romfs.sh: ffmpeg not found, using the original (heavy) sounds" >&2
    exit 0
fi

for src in ../data/sound/*.ogg; do
    dst="$out/sound/$(basename "$src")"
    # Convert once; ../data was copied above, so compare with the original.
    marker="$dst.3ds"
    if [ ! -e "$marker" ] || [ "$src" -nt "$marker" ]; then
        ffmpeg -hide_banner -loglevel error -y -i "$src" -ac 1 -ar 22050 \
            -c:a libvorbis -q:a 1 "$dst.tmp.ogg"
        mv "$dst.tmp.ogg" "$dst"
        touch "$marker"
    fi
done

# The 3dsx has to be packed again with the new romfs.
rm -f zatackax.3dsx
