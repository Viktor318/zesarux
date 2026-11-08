# ZEsarUX – magyar kiadás

[English](README.md) | **Magyar**

Ez a [ZEsarUX](https://github.com/chernandezba/zesarux) (ZX Second-Emulator
And Released for UniX, készítője César Hernández Bañó) forkja, elsősorban
windowsos használatra, magyarul vagy angolul. Magyar felülettel, a windowsos
billentyűzetkiosztással való gépeléssel, a [Fuse](https://fuse-emulator.sourceforge.net/)
módján előállított jó minőségű hanggal, átdolgozott AY zenelejátszóval és AY
keverővel, a méretét megtartó főablakkal és szabadon méretezhető gépképpel,
valamint számos javítással bővül.

Maga az emuláció az eredeti ZEsarUX. Az eredeti leírás a
[src/README_hu](src/README_hu) fájlban van (angolul: [src/README](src/README)),
az eredeti változásnapló a [src/Changelog_hu](src/Changelog_hu) fájlban. A
fork változásai a [CHANGELOG-fork.hu.md](CHANGELOG-fork.hu.md) fájlban
olvashatók.

Ez nem hivatalos ZEsarUX-kiadás: ennek a kiadásnak a hibáit kérlek itt jelezd,
ne az eredeti projektnek.


## Letöltés

Töltsd le a `ZEsarUX-13.1-SN-hu.1-windows.zip` fájlt a
[Releases](https://github.com/Viktor318/zesarux/releases) oldalról, csomagold
ki egy tetszőleges mappába, és indítsd el a `zesarux.exe`-t. Semmit nem kell
telepíteni.

A fork a ZEsarUX 13.1-SN-re épül, a készülő ZEsarUX 13.1 fejlesztői
pillanatképére („SN”: snapshot); innen a 13.1-SN-hu.1 verziónév.

## Első lépések

- Az **F5** (vagy a ZEsarUX gomb) nyitja meg a menüt.
- **Magyar menük**: *Settings > General > Language > Magyar*
  (magyarul: *beállítások > Általános > Nyelv*), vagy indítás a
  `--language hu` kapcsolóval.
- **Gépelés a windowsos billentyűzetkiosztással**: *beállítások > Hardver >
  Billentyűzet > Windowsos billentyűzetkiosztás*.
- A beállítások a `%APPDATA%\ZEsarUX` mappába (Roaming\ZEsarUX) mentődnek.

## Mit ad hozzá ez a kiadás

### Magyar felület

Minden menü, ablakcím, üzenet, súgószöveg és buboréksúgó le van fordítva; a
menük gyorsbillentyűi a magyar szavak betűi, és a menük betűkészleteiben
megvannak a magyar ékezetes betűk.

### Billentyűzet

- **Windowsos billentyűzetkiosztás** (Spectrum, ZX80, ZX81, QL és Amstrad
  CPC): amit begépelsz, az jelenik meg az emulált gépen, bármilyen a
  billentyűzeted kiosztása. Magyar billentyűzeten a Shift+2 `"`-t, az AltGr+V
  `@`-t ír; a gép billentyűkombinációját a program nyomja le.
- **Bal Alt + számbillentyűk = Caps Shift + szám** (EDIT, kurzorbillentyűk,
  DELETE...).
- A számbillentyűzet azokon a gépeken is működik, amelyeknek nincs;
  választható billentyűismétlés a ZX80-on és a ZX81-en.

### Hang

- **Jó minőségű hang** (alapból bekapcsolva): 48 kHz, 16 bit,
  sávkorlátozott, a Fuse módján előállítva (pontos beeper-időzítés, a Fuse AY
  hanggenerátora és szűrői), a Spectrumokon, az Amstrad CPC-n, a ZX80-on, a
  ZX81-en, a QL-en és a DAC-okkal.
- A Sinclair és Amstrad Spectrumok a valódi képfrissítési sebességükkel
  futnak.
- Javítások az SDL hangkimenetben (kétszer szóló rövid hangok, kihagyások
  Windowson).

### AY zenelejátszó és AY keverő

- A lejátszó az `.ay` zenéket és a Pro Tracker 3 modulokat (`.pt3`, `.vt2`)
  saját Z80-on és saját AY chipen játssza le, az
  emulált géptől függetlenül, ami közben fut tovább. A dalok hosszát megméri
  és megjegyzi, a dalok a végükön elhalkulnak, ±10 másodperces tekerés,
  *1 szám / Lista / Ismétlés* módok.
- Az **AY keverő**: sztereó mód (nyolc mód, egyéni helyekkel), csatornánként
  hangerő, panoráma és be/ki kapcsoló, fő hangerő; a beállítások dalonként
  `.mix` fájlba menthetők (ugyanaz a formátum, mint a Fuse forkban).

### Ablak és teljes képernyő

- A főablak két indítás között megtartja a méretét, a helyét és a
  maximalizált állapotát; a ZX Desktop kitölti a gép melletti és alatti
  részt.
- **A gépkép mérete** 100%, 125%, 150% vagy 200%, választható simítással; a
  menük és az ikonok mérete nem változik.
- **Teljes képernyő** ZX Desktoppal, vagy csak a gépkép, amely éles,
  egyenletes pixelekkel kitölti a képernyőt.

### Virtuális billentyűzet

Nagy felbontású billentyűzetképek három méretben (100%, 150%, 200%; a
címsorra duplán kattintva lépeget közöttük), és a gombjainak lenyomása nem
lassítja az emulációt.

A teljes lista a [CHANGELOG-fork.hu.md](CHANGELOG-fork.hu.md) fájlban van.


## Fordítás

- **Linux**: mint az eredeti ZEsarUX, lásd [src/INSTALL](src/INSTALL)
  (`./configure`, `make` az `src` mappában).
- **Windows**, keresztfordítással Linuxról vagy WSL-ből, MinGW-w64-gyel:
  `src/build-windows-cross.sh` (az elején lévő megjegyzések sorolják fel a
  szükséges csomagokat); az `src/build-windows-cross.sh --release` a kiadási
  csomagot készíti el.

A fejlesztési ág a `hungarian-ui`; a `main` az eredeti projektet követi.


## Licenc és köszönet

A ZEsarUX © César Hernández Bañó, a GNU General Public License 3-as
változata alatt (lásd [src/LICENSE](src/LICENSE) és
[src/LICENSES_info_hu](src/LICENSES_info_hu)).

A fork a ZEsarUX-on kívül ezeket használja:

- a [Fuse](https://fuse-emulator.sourceforge.net/)-ból: a hangszűrőket és az
  AY hanggenerátort (GPL), valamint a sávkorlátozott hangpuffert (Blip_Buffer,
  Shay Green, LGPL);
- Andre Weissflog [chips](https://github.com/floooh/chips) Z80-emulátorát
  (zlib licenc) az AY lejátszóhoz.
