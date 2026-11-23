# NextReg-visszaolvasás és sorszámláló teszt (ZX Spectrum Next)

`nrtest.nex` (forrás: `nrtest.asm`, fordítás: `sjasmplus nrtest.asm`; a betűkészlet a
`../dmatest/font.asm`).

## Futtatás a Next-en

1. Másold a `nrtest.nex`-et a kártya gyökerébe.
2. NextZXOS → NextBASIC: `.nexload nrtest.nex`
3. Pár másodperc múlva megjelenik az összefoglaló. Fotózd le.
4. Nyomd meg az **S**-t: a program `NRTEST.BIN` néven a kártyára menti az eredményt
   (1792 bájt). Ezt a fájlt kérem vissza.

## Mit mér

- Pillanatkép mind a 256 NextRegről, indításkor és a végén.
- Kb. 75 írható regiszterbe 4 minta (`FF`, `00`, `55`, `AA`), mindegyik után
  visszaolvasás, majd az eredeti érték visszaírása. A veszélyes bitek kimaradnak
  (50/60 Hz, scandoubler, DivMMC mapram-törlés, csupa RAM lapozás, copper indítása);
  a Pi-, billentyűtérkép-, bővítőbusz-, mag- és videómód-regisztereket csak olvassa.
- Külön esetek: `NEXTREG` és a `$243B` kiválasztás, paletta + NR `$03` 7. bit,
  vágóablak-index.
- Időzítés 3,5 MHz-en: sorok száma képkockánként, a sorszámláló az ULA-megszakítás
  után (NR `$64` = 0 és `$40`), 100 képsor hossza egy 37 T-állapotos ciklus köreiben
  (3,5 és 28 MHz).

## Várt értékek (VHDL / JNEXT alapján)

| | 50 Hz, +3/128K időzítés | 60 Hz |
|---|---|---|
| sorok/képkocka | 311 | 264 |
| sor az INT után | 248 | 224 |
| NR64=40 mellett | 1 | 24 |
| 100 sor, kör 3,5 MHz | 616 | 616 |

(48K-s időzítésnél 312 sor és 605 kör.)
