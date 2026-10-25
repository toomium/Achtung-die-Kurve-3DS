# Achtung, die Kurve! for the Nintendo 3DS (Zatacka X)

A port of [Zatacka X](https://github.com/simenheg/zatackax) to the Nintendo 3DS. The game code in
`../src` is shared with the desktop version; 3DS-specific code is guarded by `__3DS__` or lives in this
directory.

## What the port adds

- Touch menus on the bottom screen, and a pixel-font interface on both screens.
- A lineup of up to eight players on one console (people and computer players with three skill
  levels), each with a trail color, a head marker and their own buttons.
- Play with several consoles over Local Play (up to eight).
- Rules: weapons off, chosen before the game, or found as items on the field; holes; solid walls or
  a field that wraps around; duels; game speed.
- A knock-out tournament for three to eight players.
- A shop that spends the console's Play Coins on cosmetics: playfield colors and patterns, frames,
  crash effects, more trail colors and head markers. Nothing in it changes how the game plays.
- A profile with the rounds and matches won, and titles that go with them.

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

## Tests

`make -C tests` builds and runs small tests of the settings file, the computer players, the items, the
bracket of the tournament and the statistics. They only need the SDL 1.2 headers.
