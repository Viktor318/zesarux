# Changes of the Hungarian edition

**English** | [Magyar](CHANGELOG-fork.hu.md)

Changes of this fork on top of ZEsarUX 13.1-SN (the development snapshot of
the upstream `main` branch). The changes of ZEsarUX itself are in
[src/Changelog](src/Changelog) (in Hungarian: [src/Changelog_hu](src/Changelog_hu)).

## Unreleased

### Hungarian user interface

- The new upstream *Emulated Display* settings (the machine picture docked
  to the top left corner or in a window; formerly *Video Output*) in
  Hungarian.

### Keyboard

- **Windows keyboard layout also in the menus**: with the option on, the
  menus and the text fields (file names, search, numbers...) get the
  character the system types, like the emulated machine does. On a
  Hungarian keyboard Shift+2 types `"` and AltGr+V types `@` in a file name
  too; accented letters are typed without the accent. Enter, Esc, Backspace,
  the cursor keys, Tab, the F keys and the Ctrl or left Alt combinations
  work as before.

### AY music player and AY mixer

- **Pro Tracker 3 modules** (`.pt3`, also Vortex Tracker II text modules:
  `.vt2`) play in the AY player, like the `.ay` files: on the player's own
  thread, on any machine, with title, author, the exact length and where
  the music repeats, fade-out, seeking, the mixer, the audio chip windows
  and `.mid` recording. They can be opened with smart load, added to the
  playlist (also with a folder) and are in the file filters. The player
  comes from the Fuse fork (based on ZXTune). Of a TurboSound module only
  the first chip is heard for now. They need the high quality sound.
- The **AY mixer** can be opened on every machine (*Audio > AY Mixer*), also
  on those without an AY chip (48K, ZX81...): the AY player plays on any
  machine and its music goes through the mixer.
- The **audio chip windows** (registers, piano, sheet, chip info) can be
  opened also without an AY chip in the machine while the AY player has a
  song; while it has one, they show the chip of the player.
- **Audio Chip to .mid** records the music of the AY player too, also on
  machines without an AY chip: while the player has a song, its chip is
  recorded.

### Window and machine picture

- With the emulated display on a window, the machine picture size no
  longer enlarges the ZX Desktop icons and buttons in the top left corner
  (where the machine picture is): while the display is on a window that
  area is at 100%, and the chosen size comes back when it is docked again.

## v13.1-SN-hu.1 – 2026-11-06

First release of the fork.

### Hungarian user interface

- **Hungarian** as a fourth menu language (*Settings > General > Language*,
  `--language hu`): menus, window titles, messages, questions, help texts,
  tooltips, the ZX Desktop buttons, the file selector, the top bar menu and
  the footer.
- Menu hotkeys are letters of the Hungarian words; they follow the letter
  marked in the translation.
- Hungarian accented letters in the menu fonts (ZX Spectrum, AtariTOS and the
  other character sets), with the accents in the same place on every letter.
- Hungarian documentation files (`README_hu`, `Changelog_hu`, `FEATURES_hu`,
  `INCLUDEDTAPES_hu`, `LICENSES_info_hu`).

### Keyboard

- **Windows keyboard layout** (*Settings > Hardware > Keyboard*,
  `--windows-layout`, on Spectrum, ZX80, ZX81, QL and Amstrad CPC with the SDL
  driver): what you type appears on the emulated machine, whatever the layout
  of your keyboard. On a Hungarian keyboard Shift+2 types `"`, AltGr+V types
  `@`, and so on; the needed key combination of the machine is pressed
  automatically (for example Symbol shift+P on the Spectrum, SHIFT+P on the
  ZX81). Symbols that need the extended mode on the Spectrum (`[ ] { } \ | ~ ©`)
  are typed with the right key sequence. Accented vowels are typed without the
  accent; on the QL its own accented letters are used. On the CPC held keys
  repeat and the keypad keeps its own keys. It also works while the virtual
  keyboard window is active.
- **Left Alt as Caps Shift with number keys** (on by default): left Alt + 0–9
  gives Caps Shift + number (EDIT, cursor keys, DELETE...). Neither Alt is
  Symbol shift; Ctrl is.
- **Numeric keypad** on machines without one (Spectrum, ZX80/81...): the
  digits, the decimal point and Enter act as the main keyboard ones; `+ - * /`
  too, and with Num Lock off 2 4 6 8 are the cursor keys.
- **Key repeat on ZX80/81** (option, off by default, `--zx8081-key-repeat`):
  the ROMs of these machines do not repeat keys; with this option a held key
  repeats.
- QL: typing fast no longer makes a key repeat on its own (the repeat delay
  starts again with every new key).
- Windows that pass the keys to the machine also let through the keys that
  only the CPC, the PCW and the SAM have (their keypads, for example).

### Sound

- **High quality sound** (*Settings > Audio > High quality sound*, on by
  default; `--classic-audio` switches it off): 48 kHz, 16 bit, band limited
  sound made the way Fuse makes it. The beeper changes go in at their exact
  T-state (Blip_Buffer from Fuse), the AY chip is generated like in Fuse, and
  the sound goes through the Fuse filters (ULA, 48K speaker, TV). It works on
  the Spectrums, the Amstrad CPC, the ZX80, the ZX81, the QL and with the DACs
  (Specdrum, Covox, Next). A jitter buffer, fading instead of clicks and an
  adaptive rate keep the sound steady.
- The Sinclair and Amstrad Spectrums run at their real frame rate (48K:
  50.08 Hz, 128K/+2/+2A/+3: 50.02 Hz), so speed and pitch match the real
  machines.
- Fixes in the SDL sound output: short sounds (like the key click) were
  sometimes heard twice; the SDL timer ran slowly on Windows (2–3%), which
  caused dropouts; the audio device is really closed when the driver ends.
- The registers of the AY chip are 0 after a reset, as on the real chip.

### AY music player

- **New window layout**: information lines, level meters, rows of buttons,
  and a separate settings list.
- The songs play on a **Z80 and an AY chip of their own, on the sound
  thread**, apart from the emulated machine: the running program goes on and
  its sound is heard together with the music; the song goes on when the
  machine is changed. Amstrad CPC songs are recognised and play this way too,
  without slowing down the CPC.
- **Song lengths are measured** (on a separate Z80, in the background) and
  kept in a file, together with where the music repeats. Songs without a
  known length play for 5 minutes and fade out; songs that repeat fade out at
  the end, like in the Fuse fork.
- **Seeking** ±10 seconds, and a play mode row: *1 song / List / Repeat*.
- Songs without an init routine (INIT = 0) play.
- Volume and stereo settings of the player follow the AY mixer and the output
  volume of the emulator.

### AY mixer

- New **AY mixer** window: stereo mode (ABC, ACB, BAC, BCA, CBA, CAB, mono,
  custom with left/centre/right places), and the volume, panorama and on/off
  switch of each channel; a **main volume** row (the output volume of the
  emulator). Games and the AY player hear these settings. On the Amstrad CPC
  and the ATM Turbo the stereo mode is no longer fixed: the mode chosen in the
  mixer is used.
- **`.mix` files**: the mixer settings can be saved next to a song
  (`Song.mix`, or `Song.3.mix` for song 3 of a file) and loaded automatically
  with it. The format is the same as in the Fuse fork.

### Window, picture size and full screen (SDL2)

- The **main window keeps its size** when the machine, the zoom, the border or
  the footer changes; ZX Desktop fills the rest of the window. The menus,
  icons and texts keep their size on every machine.
- **Machine picture size** 100%, 125%, 150% or 200% (zoom buttons, *Machine
  picture size*, `--machine-scale`); the largest that fits is used. Optional
  **smoothing** for the sizes that do not give whole pixels
  (`--machine-smooth`).
- The main window **remembers its position and size** (and if it was
  maximized); the first start opens it maximized. If the saved place is not
  visible on any display, it opens maximized.
- ZX Desktop: the upper buttons wrap into rows, the lower buttons are on the
  left of the strip below the machine, the icons are rearranged when they
  would be out of place; icons can be dragged to change their order.
- **Full screen** with ZX Desktop (the same layout as the window) or without
  it: then the machine picture **fills the screen** with sharp bilinear
  scaling (even pixels, keeping the aspect ratio).
- The full screen button reacts to every press (it ignored presses for 3
  seconds after a switch).
- New F-key actions: zoom in and out (the machine picture size) and ZX
  Desktop on/off (by default F8, F7 and F9).
- The **process switcher** (in Hungarian: *Ablakváltó*) is kept at the bottom
  right (it was bottom left, where the lower buttons are now), also after
  switching full screen on and off.

### Virtual keyboard

- The keyboard picture is scaled to the window size; three sizes in the Help
  menu: **100%, 150% and 200%** of the original picture; a double click on the
  window title steps between them.
- High resolution keyboard pictures (twice the original size), scaled
  smoothly.
- The picture is scaled once and kept, so pressing keys on the virtual
  keyboard no longer slows down the emulation.

### Windows

- **Settings in `%APPDATA%\ZEsarUX`** (Roaming\ZEsarUX): the configuration
  file, the menu search index, the AY song lengths and the mixer settings.
  Files of earlier versions in the home folder are copied there the first
  time.
- Number dialogs: the slider follows the mouse only while the button is held.
- Cross-build of the Windows version from Linux/WSL with MinGW-w64
  (`src/build-windows-cross.sh`).
