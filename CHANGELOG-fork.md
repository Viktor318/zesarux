# Changes of the Hungarian edition

**English** | [Magyar](CHANGELOG-fork.hu.md)

Changes of this fork on top of ZEsarUX 13.1 (the development version of the
upstream `main` branch; v13.1-B1-hu.1 is based on 13.1 Beta 1). The changes of ZEsarUX itself are in
[src/Changelog](src/Changelog) (in Hungarian: [src/Changelog_hu](src/Changelog_hu)).

## Unreleased

### Virtual keyboard

- **ZX Spectrum Next keyboard drawn again**: a drawing instead of the photo,
  after the real keyboard of the Next (the words above the keys, the two
  word keys such as NEW/STOP, the block graphics, BLACK in its box), at the
  same size and with the keys at the same places. Made by
  `tools/keyboards/next.py`.
- **PNG keyboard pictures**: the program first looks for the `.png` picture
  (with the same name) in the `keyboards` folder, and only if there is none
  for the `.bmp`. The PNG is read by our own code with the miniz library
  already in the program; palette, grey, RGB and RGBA PNGs all work (8 bit,
  at most 256 colours).
- The pictures drawn again (the common machines, CPC 464 and Next) are
  stored **at one and a half times the resolution**, as PNG: 1620 pixels wide
  (the CPC 1920), three times the original picture of the machine. So 150% is
  sharp too (one pixel of two), and the larger sizes stay sharp. With these
  pictures the `keyboards` folder is about 6 MB smaller.
- **Two new sizes: 250% and 300%** in the Help menu and with a double click on
  the title (100 → 125 → 150 → 175 → 200 → 250 → 300 → 100%). At 300% the
  pictures drawn again are shown pixel by pixel. The double click skips a
  size that does not fit on the screen and goes to 100%.
- **All keyboard pictures are PNG**: the pictures of the other machines too
  (lossless, the colours and the order of the palette unchanged). The
  `keyboards` folder went from 20 MB to 6.8 MB. New BMP pictures from upstream
  still work.
- Fixed: the yellow highlight of a key sometimes stayed in the virtual
  keyboard window until the mouse came back to it. It now goes away when the
  mouse leaves the window, and when the window goes to the background or is
  closed.

### ZX Spectrum Next

- **More complete mouse on the Next**: the Kempston mouse (Settings →
  Hardware → Mouse → Kempston Mouse) passes the wheel and the middle button to
  the Next too, like the real Next (bits 7–4 of the buttons port are the wheel
  counter, bit 2 is the middle button). The button swap bit of Next register
  0x0A (left and right button swapped) is followed as well. With the menu open
  the wheel stays with the menu.
- **The AY of the Next is as loud as on the 128K** (high quality sound): all
  three AY chips of the Next are on (TurboSound), and until now each chip was
  divided by the number of chips, so a program using only one chip (for
  example the PLAY of NextBASIC) played at a third of the 128K volume. Now the
  chips are added up, like on the real Next (classic sound unchanged).
- **The DACs of the Next start from the middle value (0x80)**, and go back to
  it on reset too, like on the real Next. Until now they started from 0: the
  unused DACs sat at the lowest value, which gave a hidden offset and a bigger
  click at the first DAC write.
- **No turbo limit in the Next ROM by default**: “Limit turbo on ROM”
  (Settings → Hardware → CPU) is off by default, so `RUN AT 3` of NextBASIC
  really runs at 28 MHz (until now it was limited to 14 MHz in the ROM). A
  saved setting is kept: if the limit was saved before, switch it off once in
  the menu. The `--help` text is fixed too (the default of
  `--tbblue-max-turbo-rom` is 4).

### Storage

- **Persistent writes of MMC and IDE images write only the changed sectors**:
  the program remembers which 512 byte sectors the machine wrote, and the
  save every second writes only these at their place in the file (after the
  .hdf header), neighbouring sectors together. Until now every change wrote
  the whole image: with a 2 GB Next card a save or a change of settings meant
  2 GB of writing and a noticeable freeze, now a few kilobytes (a NextBASIC
  SAVE for example 2.5 KB). If the file is missing or smaller than expected,
  the whole image is written as before.
- The warning for images larger than 1 GB says so now: “MMC bigger than 1 GB:
  slower to load and uses as much RAM” (the image is still loaded into memory
  as a whole).

## v13.1-B1-hu.1 – 2026-11-18

The second release of the fork, based on ZEsarUX 13.1 Beta 1.

### Hungarian user interface

- The new upstream *Emulated Display* settings (the machine picture docked
  to the top left corner or in a window; formerly *Video Output*) in
  Hungarian.
- The translation follows the common glossary of the four forks:
  - *Browse*: Böngészés → **Tallózás** (the windows named browser, e.g.
    Kazettaböngésző, stay);
  - *Cartridge*: cartridge → **kártya**, for ROMs **ROM-kártya** (e.g.
    *Kártya betöltése*, *Kártya kivétele*, *Timex kártya*);
  - the *Display* menu: Kijelző → **Megjelenítés** (hotkey: j; „kijelző”
    meaning the screen stays, e.g. *Emulált kijelző*);
  - *Stop emulation in menus*: megállítás → leállítás; *Autofire*: automata
    tűz → automatikus tűz; *Disabled save settings*: tiltva → kikapcsolva.
- The Transtape *Simulate reset press* menu item and window title are
  „Reset gomb megnyomása” and „Reset gomb” (they were „Újraindító gomb”), to
  match the „OK. Reset gomb megnyomva” text of the window.

### Keyboard

- **Windows keyboard layout also in the menus**: with the option on, the
  menus and the text fields (file names, search, numbers...) get the
  character the system types, like the emulated machine does. On a
  Hungarian keyboard Shift+2 types `"` and AltGr+V types `@` in a file name
  too; accented letters are typed without the accent. Enter, Esc, Backspace,
  the cursor keys, Tab, the F keys and the Ctrl or left Alt combinations
  work as before.

### Virtual keyboard

- **Keyboard pictures drawn again**, with the same drawings as in the
  EightyOne fork: Spectrum 16K/48K, Spectrum+ and 128, +2, +2A/+3, ZX80
  (with INTEGRAL FUNCTIONS under it), ZX81 (TK82C, TK83, CZ1000), Timex
  Sinclair 1000 (its own picture, with the TIMEX name), TS1500, TK85,
  Jupiter Ace, TC2048 and TS2068. They are 1080 pixels wide and drawn at
  that size, so 200% is sharp pixel for pixel. The places of the keys are
  the ones of the tables until now, moved onto the keys of the drawings.
  The 16K shows the picture of the 48K, as in EightyOne. The pictures of
  the Spanish 48+/128, the Inves and the other machines did not change.
- **Five sizes** in the Help menu and with a double click on the title:
  100%, 125%, 150%, 175% and 200% (they were 100, 150 and 200%), as in
  EightyOne.
- On the Spectrum+ and 128 picture the `!` is a little thicker and the `'` a
  little thinner, and the `@` and the `#` are in the font's own proportions
  (they were squeezed and stretched).
- **Amstrad CPC 464 keyboard drawn again** (the CPC 4128 shows it too): a
  drawing instead of the photo, at the same size (1280×490), in the style of
  the EightyOne pictures. The places of the keys are measured on the photo
  and the key table is moved onto them; both parts of the L-shaped ENTER can
  be clicked. The texts are bold, so that they do not blur when the picture
  is made smaller; the small signs (`*` `+` `-` `=` `_` `<` `>`) are of
  normal size.
  Made by `tools/keyboards/cpc464.py`.
- The pictures are made from the drawings of the EightyOne fork
  (`Source/KBPics/svg`) by `tools/keyboards/make_zesarux_keyboards.py`.
- When it is first opened, the window fits the picture at 200%. The new
  pictures are taller for their width than the old ones, so with a window
  size saved earlier there can be an empty band beside the picture; a
  double click on the title fits the window to the picture again.

### Upstream

- Merged the upstream state of 2026-11-18: **ZX Desktop is renamed DAVID
  Desktop** (in the English interface), ATM Turbo memory zones for the
  debugger, small fixes. In the Hungarian interface the name stays **ZX
  Desktop** (Viktor's choice); the translations follow the new English
  texts, so they stay in Hungarian. The fork's own English texts (e.g. the
  help of the machine picture size, the tooltip of the desktop switch button)
  say DAVID Desktop too. The command line options (`--enable-zxdesktop` etc.)
  and the saved settings did not change.
- Merged upstream 13.1 Beta 1 (2026-11-17): **ATM Turbo 1/2 and ATM Turbo
  2+/3 virtual keyboards**, and the fix for the row padding of BMP pictures
  (pictures whose width is not a multiple of four were shown slanted; the
  fork's keyboard scaler now reads them the same way). The program now
  shows its version as 13.1-B1-hu.1. The fork keeps its own keyboard
  scaling (100–200%, any window size, smoothing, stored picture)
  instead of the new upstream integer-factor scaling.

### Sound

- The *High quality sound* setting is gone: the high quality sound is
  always used where it is available (with the SDL2 audio driver, the
  default one and the one on Windows). It was already on by default, and
  there is no reason to go back to the old 8 bit sound. The machines it
  does not make yet still convert their classic sound to it. The
  `--classic-audio` command line option stays for testing, but it is no
  longer saved in the configuration.

### AY music player and AY mixer

- **Pro Tracker 3 modules** (`.pt3`, also Vortex Tracker II text modules:
  `.vt2`) play in the AY player, like the `.ay` files: on the player's own
  thread, on any machine, with title, author, the exact length and where
  the music repeats, fade-out, seeking, the mixer, the audio chip windows
  and `.mid` recording. They can be opened with smart load, added to the
  playlist (also with a folder) and are in the file filters. The player
  comes from the Fuse fork (based on ZXTune). Of a TurboSound module only
  the first chip is heard for now. They need the high quality sound (the
  SDL2 audio driver).
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
- **Size of the emulated display on a window** as a setting of its own:
  100%, 125%, 150% or 200% (*Main Window > Size on a window*,
  `--emulated-display-scale`), apart from the docked machine picture size.
  While the display is on a window, the zoom buttons change it and the
  window follows the picture; a larger size is used only if the window
  fits. Sizes that do not give whole pixels are smoothed by *Smooth machine
  picture*.
- With the display on a window, full screen without ZX Desktop stayed black
  (switching ZX Desktop off closed the window, but the display was still
  taken as windowed); now the machine picture fills the screen, and the
  window opens again when leaving full screen.
- The emulated display on a window no longer slows the machine down to half
  speed: it drew the whole picture again on every frame, and the emulation
  fell to half speed (slow typing, lost keys, slow key repeat). Now only the
  changed pixels are drawn.
- The new upstream settings of the windowed emulated display, in Hungarian
  and with the fork's drawing: *Fit image to window* (the picture follows
  the window size, smoothed if *Smooth machine picture* is on), *Keep
  proportions* and *Autoresize* (on a machine change the window follows the
  picture at the chosen size). With fitting, the zoom buttons size the
  window for a 100/125/150/200% picture, and the picture follows it.
- On the Amstrad CPC and the ZX Spectrum Next (TBBlue) the machine picture
  stayed in the top left corner too with the emulated display on a window
  (double picture); now it is only in the window.
- With the display on a window, a machine change (with Autoresize, or the
  zoom buttons) turned the windows above the emulated display, like the
  splash message showing the screen mode, into empty blue boxes; now every
  window is drawn again after the resize.
- Smoothing the machine picture (for sizes that do not give whole pixels,
  like 125%) uses less CPU time: only the machine picture area is
  processed, without copying the whole window picture on every frame.
- Moving the mouse over an open menu no longer slows the machine down: on
  every mouse movement (every change of the selected line) the menu drew the
  whole screen and the whole ZX Desktop again, which brought the machine
  below real speed and made the sound stutter. In multitask mode the screen
  is refreshed on every frame anyway, so this is skipped now.
- Opening and closing the menu, activating a window (clicking its title or
  the machine) and closing a window no longer stall the machine. Each of
  these drew the whole ZX Desktop again, and usually the whole machine
  picture too. Now, on the SDL2 display, the machine layer is always up to
  date, so it is enough to mix the two layers again where the menus were.
  The ZX Desktop is only drawn again when something that changes its look
  has changed (a setting, an icon, a button). The close-all-menus button
  in the top row disappears by putting back the background saved under
  it. A click only draws the frame around the machine again. Drawing menus
  and windows is also faster, as their size is no longer worked out again
  for every pixel.
- More stalls are gone in multitask mode:
  - Closing a menu **tooltip** drew every window again twice, plus the
    whole screen and the whole ZX Desktop. Now only the windows are drawn
    again, once.
  - Menu **hotkeys** and opening the **file selector** no longer draw the
    whole screen and the ZX Desktop again.
  - **Dragging an icon** draws the ZX Desktop again on every mouse movement
    only where the icon was and where it went. The full redraw took about
    25 ms here; now it is about 4 ms.
  - A dropped icon no longer stays where it was dropped if it then goes
    somewhere else.
- The open-window mark under the ZX Desktop icons and the lower buttons
  (drives, tape, disk) are also kept up to date after the menus are closed.

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
