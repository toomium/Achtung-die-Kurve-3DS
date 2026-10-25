# The Nintendo 3DS port: notes

The main README is [../README.md](../README.md): what the game is, how to install it, and the
features. This page is for building and working on the port.

The game code in `../src` is shared with the desktop version. 3DS-only code is either guarded by
`__3DS__` or lives in files that end in `3ds.c` (they are compiled for the 3DS only), or in this
directory.

## Layout

- `Makefile`: the 3DS build. It compiles `../src` and `source/`.
- `source/platform.c`: the only file that may include `<3ds.h>`.
- `make-romfs.sh`: puts `../data` and `assets/` together into `romfs-build/`, and converts the sounds
  to 22.05 kHz mono if `ffmpeg` is there.
- `assets/`: files for the 3DS only (the pixel font, the logo). They are laid over the game's data.
- `app.rsf`, `banner.png`, `icon.png`, `tools/`: the CIA (`tools/make-cia.sh`) and the logo, banner and
  icon (`tools/make-logo.py`, needs Pillow).
- `screenshots/`: the pictures of the main README.

## Building

    export DEVKITPRO=/opt/devkitpro
    export DEVKITARM=$DEVKITPRO/devkitARM
    export PORTLIBS=$DEVKITPRO/portlibs/3ds
    cd 3ds
    make

Needs the portlibs `3ds-sdl`, `3ds-sdl_image`, `3ds-sdl_mixer` and `3ds-sdl_ttf`.

## Notes

- Setting `debug = 1` in `sdmc:/3ds/zatackax/settings.cfg` writes a log to `sdmc:/zatackax.log`.
- Without `dspfirm.cdc` the game runs silent.
- Never write pixels as raw bytes; use `SDL_MapRGB` and `SDL_GetRGB`, the channel order of the 3DS
  differs.
- Close the sounds and fonts before `platformExit()`, or the exit crashes.
