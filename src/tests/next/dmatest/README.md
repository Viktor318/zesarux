# DMA-időzítés teszt (ZX Spectrum Next)

`dmatest.nex` a zxnDMA (port $6B) idejét méri a képsorszámlálóval (NextReg $1E/$1F,
egy sor kb. 64 µs), 3,5 és 28 MHz-en, és a végén egy táblázatot ír ki. Forrás:
`dmatest.asm` (sjasmplus: `sjasmplus dmatest.asm`), a betűkészlet `font.asm`
(font8x8_basic, Daniel Hepper, public domain).

- A2/A3/A4: folyamatos mód, előosztó nélkül, 2/3/4 ciklusos időzítés (4 KB ill. 16 KB)
- B8: folyamatos mód, előosztó 8, 512 bájt
- C8: löketes (burst) mód, előosztó 8, 512 bájt
- D2: löketes mód, előosztó 2, 4096 bájt
- E: hang, löketes mód, előosztó 55, 16 KB négyszögjel a $DF DAC-portra (kb. 1 mp, ~440 Hz)

„sor” = eltelt képsorok, „kor” = a processzor várakozó ciklusának köre (folyamatos
módban a processzor áll, ezért ott 1). 60 Hz-en egy képkocka 264 soros (VHDL), a 264-nél
hosszabb mérés körbefordul (D2 3,5 MHz-en).

## Mérések egy KS3-on (2026-11-21)

| | 3.02.04, 60 Hz | 3.03.03, 60 Hz | 3.03.03, 50 Hz |
|---|---|---|---|
| A2 | 73 / 45 | 74 / 45 | 73 / 45 |
| A3 | 109 / 54 | 109 / 54 | 109 / 54 |
| A4 | 145 / 72 | 145 / 72 | 145 / 72 |
| B8 | 80 / 73 | 80 / 72 | 80 / 72 |
| C8 | 90 (342) / 73 (2386) | 90 (342) / 74 (2940) | 90 (342) / 74 (2940) |
| D2 | 16+264 (513) / 161 (4608) | 16+264 (513) / 160 (5632) | 280 (513) / 160 (5632) |
| E (képkocka) | 62 | 62 | 52 |

(3,5 MHz / 28 MHz, zárójelben a körök száma.)

Ebből: bájtonként az olvasás és az írás ciklushossza (2+2, 3+3, 4+4 órajel; 28 MHz-en a
2 ciklusos memóriaolvasás 3); előosztóval bájtonként legalább előosztó × 32 db 28 MHz-es
órajel, a processzor sebességétől függetlenül; folyamatos módban a processzor áll, löketes
módban előosztóval a várakozás alatt fut. Ezt követi a ZEsarUX (`datagear.c`, TBBlue).

A sorok a NextZXOS alatti +3-as időzítés (228 T/sor, 50 Hz-en 311 sor) szerint értendők;
a ZEsarUX 2026-11-23 előtt 224 T-s sorokkal futott, ezért 3,5 MHz-en kb. 2 sorral többet mért.
