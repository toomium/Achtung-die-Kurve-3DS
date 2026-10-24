# Zatacka X for the Nintendo 3DS

A port of [Zatacka X](https://github.com/simenheg/zatackax) to the Nintendo 3DS. The game code in
`../src` is shared with the desktop version; 3DS-specific code is guarded by `__3DS__` or lives in this
directory.

## Requirements

- [devkitPro](https://devkitpro.org/) with devkitARM and libctru
- The 3DS portlibs `3ds-sdl`, `3ds-sdl_image`, `3ds-sdl_mixer` and `3ds-sdl_ttf` (SDL 1.2)
- `ffmpeg` (optional): converts the sounds to a lighter format for the 3DS

## Building

    export DEVKITPRO=/opt/devkitpro
    export DEVKITARM=$DEVKITPRO/devkitARM
    export PORTLIBS=$DEVKITPRO/portlibs/3ds
    cd 3ds
    make

This produces `zatackax.3dsx`. Run it with Homebrew on a 3DS or in an emulator such as Azahar.

Settings are stored in `sdmc:/3ds/zatackax/settings.cfg`. Setting `debug = 1` there writes a log to
`sdmc:/zatackax.log`. Sound needs the DSP firmware dumped from your own console
(`sdmc:/3ds/dspfirm.cdc`); without it the game runs silent.

## License

GPLv3 or later, like the original. See [COPYING](../COPYING). The pixel font in `assets/fonts` is
licensed under the SIL Open Font License, see `assets/fonts/OFL.txt`.
