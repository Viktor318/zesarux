# A magyar kiadás változásai

[English](CHANGELOG-fork.md) | **Magyar**

A fork változtatásai a ZEsarUX 13.1-SN-hez képest (az upstream `main` ág
fejlesztői pillanatképe). A ZEsarUX saját változásai a
[src/Changelog_hu](src/Changelog_hu) fájlban vannak (angolul:
[src/Changelog](src/Changelog)).

## Kiadatlan

### Billentyűzet

- **Windowsos billentyűzetkiosztás a menükben is**: bekapcsolt beállítás
  mellett a menük és a szövegmezők (fájlnevek, keresés, számok...) azt a
  karaktert kapják, amit a rendszer ír, ahogy az emulált gép is. Magyar
  billentyűzeten a Shift+2 fájlnévben is `"`-t, az AltGr+V `@`-t ír; az
  ékezetes betűk ékezet nélkül mennek át. Az Enter, az Esc, a Backspace, a
  kurzorbillentyűk, a Tab, az F-billentyűk és a Ctrl vagy bal Alt
  kombinációk ugyanúgy működnek, mint eddig.

## v13.1-SN-hu.1 – 2026-11-06

A fork első kiadása.

### Magyar felület

- **Magyar** nyelv negyedikként a menükben (*beállítások > Általános > Nyelv*,
  `--language hu`): menük, ablakcímek, üzenetek, kérdések, súgószövegek,
  buboréksúgók, a ZX Desktop gombjai, a fájlválasztó, a felső menüsor és a
  lábléc.
- A menük gyorsbillentyűi a magyar szavak betűi; a fordításban megjelölt
  betűt követik.
- Magyar ékezetes betűk a menük betűkészleteiben (ZX Spectrum, AtariTOS és a
  többi), minden betűn ugyanott az ékezettel.
- Magyar dokumentációs fájlok (`README_hu`, `Changelog_hu`, `FEATURES_hu`,
  `INCLUDEDTAPES_hu`, `LICENSES_info_hu`).

### Billentyűzet

- **Windowsos billentyűzetkiosztás** (*beállítások > Hardver > Billentyűzet*,
  `--windows-layout`; Spectrumon, ZX80-on, ZX81-en, QL-en és Amstrad CPC-n,
  SDL meghajtóval): amit begépelsz, az jelenik meg az emulált gépen, bármilyen
  a billentyűzeted kiosztása. Magyar billentyűzeten a Shift+2 `"`-t, az
  AltGr+V `@`-t ír, és így tovább; a gép szükséges billentyűkombinációját a
  program nyomja le (például Spectrumon Symbol shift+P, ZX81-en SHIFT+P). A
  Spectrumon kiterjesztett módot igénylő jelek (`[ ] { } \ | ~ ©`) a megfelelő
  billentyűsorrenddel íródnak be. Az ékezetes magánhangzók ékezet nélkül
  mennek át; QL-en a QL saját ékezetes betűi jelennek meg. CPC-n a nyomva
  tartott billentyű ismétel, a számbillentyűzet a saját billentyűit adja.
  Akkor is működik, ha a virtuális billentyűzet ablaka az aktív.
- **Bal Alt = Caps Shift a számoknál** (alapból bekapcsolva): a bal Alt + 0–9
  Caps Shift + számot ad (EDIT, kurzorbillentyűk, DELETE...). Egyik Alt sem
  Symbol shift; az a Ctrl.
- **Számbillentyűzet** azokon a gépeken, amelyeknek nincs (Spectrum,
  ZX80/81...): a számok, a tizedespont és az Enter a fő billentyűzetéként
  működik; a `+ - * /` is, és kikapcsolt Num Locknál a 2 4 6 8 a
  kurzorbillentyűk.
- **Billentyűismétlés ZX80/81-en** (beállítás, alapból kikapcsolva,
  `--zx8081-key-repeat`): ezeknek a gépeknek a ROM-ja nem ismétli a
  billentyűket; a beállítással a nyomva tartott billentyű ismétel.
- QL: gyors gépelésnél nem kezd el magától ismételni egy betű (az ismétlés
  késleltetése minden új billentyűnél újraindul).
- A billentyűket a gépnek továbbadó ablakok azokat a billentyűket is
  átengedik, amelyek csak a CPC-n, a PCW-n és a SAM-on vannak (például a
  számbillentyűzetük).

### Hang

- **Jó minőségű hang** (*beállítások > Hang > Jó minőségű hang*, alapból
  bekapcsolva; `--classic-audio` kikapcsolja): 48 kHz-es, 16 bites,
  sávkorlátozott hang, ugyanúgy előállítva, mint a Fuse-ban. A beeper
  változásai pontosan a saját T-állapotukban kerülnek be (Blip_Buffer a
  Fuse-ból), az AY chip hangja a Fuse módján készül, és a hang a Fuse
  szűrőin megy át (ULA, 48K hangszóró, TV). Működik a Spectrumokon, az
  Amstrad CPC-n, a ZX80-on, a ZX81-en, a QL-en és a DAC-okkal (Specdrum,
  Covox, Next). Kiegyenlítő puffer, kattanás helyett elhalkulás és
  alkalmazkodó mintavételi sebesség tartja egyenletesen a hangot.
- A Sinclair és Amstrad Spectrumok a valódi képfrissítési sebességükkel
  futnak (48K: 50,08 Hz, 128K/+2/+2A/+3: 50,02 Hz), így a sebesség és a
  hangmagasság a valódi gépekével egyezik.
- Javítások az SDL hangkimenetben: a rövid hangok (például a billentyűkattanás)
  néha kétszer szóltak; az SDL időzítő Windowson lassabban járt (2–3%), ami
  kihagyásokat okozott; a hangeszköz valóban bezárul, amikor a meghajtó leáll.
- Az AY chip regiszterei reset után 0-k, mint a valódi chipen.

### AY lejátszó

- **Új ablakelrendezés**: információs sorok, szintjelzők, gombsorok és külön
  beállításlista.
- A dalok **saját Z80-on és saját AY chipen, a hang szálán** szólnak, az
  emulált géptől függetlenül: a futó program megy tovább, a hangja a zenével
  együtt hallatszik, és gépváltáskor a dal szól tovább. Az Amstrad CPC-s
  dalokat is felismeri és így játssza le, a CPC lassítása nélkül.
- **A dalok hosszát megméri** (külön Z80-on, a háttérben), és fájlban tárolja,
  azzal együtt, hogy hol ismétlődik a zene. Az ismeretlen hosszú dalok 5 percig
  szólnak, és elhalkulnak; az ismétlődő dalok a végükön elhalkulnak, mint a
  Fuse forkban.
- **Tekerés** ±10 másodperccel, és lejátszási mód sor: *1 szám / Lista /
  Ismétlés*.
- Az inicializáló rutin nélküli dalok (INIT = 0) is megszólalnak.
- A lejátszó hangereje és sztereó beállítása követi az AY keverőt és az
  emulátor kimeneti hangerejét.

### AY keverő

- Új **AY keverő** ablak: sztereó mód (ABC, ACB, BAC, BCA, CBA, CAB, mono,
  egyéni, bal/közép/jobb helyekkel), csatornánként hangerő, panoráma és be/ki
  kapcsoló; **Fő hangerő** sor (az emulátor kimeneti hangereje). A játékok és
  az AY lejátszó is ezekkel a beállításokkal szólnak. Az Amstrad CPC-n és az
  ATM Turbón a sztereó mód nincs többé rögzítve: a keverőben választott mód
  érvényes.
- **`.mix` fájlok**: a keverő beállításai a dal mellé menthetők (`Dal.mix`,
  vagy egy fájl 3. dalához `Dal.3.mix`), és a dallal együtt automatikusan
  betöltődnek. A formátum ugyanaz, mint a Fuse forkban.

### Ablak, gépkép mérete és teljes képernyő (SDL2)

- **A főablak megtartja a méretét**, amikor a gép, a nagyítás, a border vagy
  a lábléc változik; a ZX Desktop kitölti az ablak többi részét. A menük, az
  ikonok és a szövegek minden gépen ugyanakkorák.
- **A gépkép mérete** 100%, 125%, 150% vagy 200% (a nagyítógombok, *Gépkép
  mérete*, `--machine-scale`); a legnagyobb, amelyik elfér. Választható
  **simítás** azokhoz a méretekhez, amelyek nem adnak egész pixeleket
  (`--machine-smooth`).
- A főablak **megjegyzi a helyét és a méretét** (és hogy maximalizálva volt-e);
  az első indításkor maximalizálva nyílik. Ha a mentett hely egyik képernyőn
  sem látszik, maximalizálva nyílik.
- ZX Desktop: a felső gombsor sorokba törik, az alsó gombsor a gép alatti
  sáv bal szélén van, az ikonok átrendeződnek, ha rossz helyre kerülnének;
  az ikonok húzással átrendezhetők.
- **Teljes képernyő** ZX Desktoppal (ugyanaz az elrendezés, mint ablakban)
  vagy anélkül: ilyenkor a gépkép **kitölti a képernyőt** éles simítással
  (egyenletes pixelek, megtartott képarány).
- A teljes képernyő gomb minden nyomásra vált (a váltás után 3 másodpercig
  nem reagált).
- Új F-billentyű műveletek: nagyítás és kicsinyítés (a gépkép mérete) és a
  ZX Desktop be/ki (alapból F8, F7 és F9).
- Az **Ablakváltó** (Process switcher) a jobb alsó sarokban marad (eddig bal
  alul volt, ahol most az alsó gombsor van), a teljes képernyő be- és
  kikapcsolása után is.

### Virtuális billentyűzet

- A billentyűzet képe az ablak méretéhez igazodik; három méret a Súgó
  menüben: az eredeti kép **100%-a, 150%-a és 200%-a**; az ablak címsorára
  duplán kattintva lépeget közöttük.
- Nagy felbontású billentyűzetképek (az eredeti kétszerese), simított
  méretezéssel.
- A program a képet egyszer méretezi és eltárolja, így a virtuális
  billentyűzeten lenyomott gombok nem lassítják az emulációt.

### Windows

- **A beállítások a `%APPDATA%\ZEsarUX` mappában** (Roaming\ZEsarUX): a
  beállításfájl, a menükereső indexe, az AY dalok hossza és a keverő
  mentései. A korábbi változatok fájljai a felhasználói mappából az első
  alkalommal átmásolódnak ide.
- Számbeállító ablakok: a csúszka csak lenyomott egérgombbal követi az
  egeret.
- A Windowsos változat fordítása Linuxról/WSL-ből, MinGW-w64-gyel
  (`src/build-windows-cross.sh`).
