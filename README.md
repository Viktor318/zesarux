# ZEsarUX – Hungarian edition

**English** | [Magyar](README.hu.md)

This is a fork of [ZEsarUX](https://github.com/chernandezba/zesarux), the
ZX Second-Emulator And Released for UniX by César Hernández Bañó, made mainly
for using it on Windows, in Hungarian or English. It adds a Hungarian user
interface, typing with the Windows keyboard layout, high quality sound made
the way [Fuse](https://fuse-emulator.sourceforge.net/) makes it, a reworked
AY music player with an AY mixer, a main window that keeps its size with a
freely sized machine picture, and several fixes.

The emulation itself is the original ZEsarUX. The original documentation is
in [src/README](src/README) (in Hungarian: [src/README_hu](src/README_hu)),
the original change log in [src/Changelog](src/Changelog). The changes of
this fork are listed in [CHANGELOG-fork.md](CHANGELOG-fork.md).

This is not an official ZEsarUX release: please report problems of this
edition here, not to the original project.


## Download

Download `ZEsarUX-13.1-SN-hu.1-windows.zip` from the
[Releases](https://github.com/Viktor318/zesarux/releases) page, unpack it
into a folder of your choice and start `zesarux.exe`. Nothing has to be
installed.

The fork is based on ZEsarUX 13.1-SN, the development snapshot ("SN") of the
coming ZEsarUX 13.1; hence the version name 13.1-SN-hu.1.

## First steps

- **F5** (or the ZEsarUX button) opens the menu.
- **Hungarian menus**: *Settings > General > Language > Magyar*, or start it
  with `--language hu`.
- **Typing with the Windows keyboard layout**: *Settings > Hardware >
  Keyboard > Use Windows keyboard layout*.
- The settings are saved in `%APPDATA%\ZEsarUX` (Roaming\ZEsarUX).

## What this edition adds

### Hungarian user interface

Every menu, window title, message, help text and tooltip is translated; the
menu hotkeys are letters of the Hungarian words, and the menu fonts have the
Hungarian accented letters.

### Keyboard

- **Windows keyboard layout** (Spectrum, ZX80, ZX81, QL and Amstrad CPC):
  what you type appears on the emulated machine, whatever the layout of your
  keyboard. On a Hungarian keyboard Shift+2 types `"`, AltGr+V types `@`;
  the key combination of the machine is pressed automatically.
- **Left Alt + number keys = Caps Shift + number** (EDIT, cursor keys,
  DELETE...).
- The numeric keypad works on machines without one; optional key repeat on
  the ZX80 and ZX81.

### Sound

- **High quality sound** (on by default): 48 kHz, 16 bit, band limited, made
  the way Fuse makes it (exact beeper timing, the Fuse AY generator and
  filters), on the Spectrums, the Amstrad CPC, the ZX80, the ZX81, the QL
  and with the DACs.
- The Sinclair and Amstrad Spectrums run at their real frame rate.
- Fixes in the SDL sound output (doubled short sounds, dropouts on Windows).

### AY music player and AY mixer

- The player plays `.ay` music on a Z80 and an AY chip of its own, apart from
  the emulated machine, which keeps running. Song lengths are measured and
  kept, songs fade out at the end, seeking ±10 seconds, *1 song / List /
  Repeat* modes.
- The **AY mixer**: stereo mode (eight modes, with custom places), volume,
  panorama and on/off switch of each channel, main volume; settings can be
  saved for each song in `.mix` files (the same format as in the Fuse fork).

### Window and full screen

- The main window keeps its size, place and maximized state between runs;
  ZX Desktop fills the space beside and below the machine.
- **Machine picture size** 100%, 125%, 150% or 200%, with optional
  smoothing; the menus and icons keep their size.
- **Full screen** with ZX Desktop, or the machine picture alone filling the
  screen with sharp, even pixels.

### Virtual keyboard

High resolution keyboard pictures in three sizes (100%, 150%, 200%; a double
click on the title steps between them), and pressing its keys no longer slows
down the emulation.

See [CHANGELOG-fork.md](CHANGELOG-fork.md) for the full list.


## Building

- **Linux**: as the original ZEsarUX, see [src/INSTALL](src/INSTALL)
  (`./configure`, `make` in `src`).
- **Windows**, cross-built from Linux or WSL with MinGW-w64:
  `src/build-windows-cross.sh` (the comments at its start list the needed
  packages); `src/build-windows-cross.sh --release` makes the release
  package.

The development branch is `hungarian-ui`; `main` follows the original
project.


## Licence and credits

ZEsarUX is © César Hernández Bañó, licensed under the GNU General Public
License version 3 (see [src/LICENSE](src/LICENSE) and
[src/LICENSES_info](src/LICENSES_info)).

This fork uses, besides ZEsarUX:

- from [Fuse](https://fuse-emulator.sourceforge.net/): the sound filters and
  the AY sound generator (GPL), and the band limited sound buffer
  (Blip_Buffer by Shay Green, LGPL);
- the [chips](https://github.com/floooh/chips) Z80 emulator by Andre
  Weissflog (zlib licence) for the AY player.
