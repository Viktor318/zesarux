; =============================================================================
; ZX Spectrum Next - DMA idozites teszt (v1)
; A zxnDMA (port $6B) idejet meri a kepsorszamlaloval (NextReg $1E/$1F,
; egy sor kb. 64 mikroszekundum), 3,5 MHz-en es 28 MHz-en.
;
; Meresek (forras: $C000, cel: $4000, mindketto sajat 8K-s lapokon, a
; kepernyon kivul, igy nincs ULA-kontencio):
;   A2/A3/A4 folyamatos mod, elooszto nelkul, 2/3/4 ciklusos idozites
;            (3,5 MHz: 4096 bajt, 28 MHz: 16384 bajt)
;   B8       folyamatos mod, elooszto 8, 512 bajt
;   C8       loketes (burst) mod, elooszto 8, 512 bajt
;   D2       loketes mod, elooszto 2, 4096 bajt
;   E        hang: loketes mod, elooszto 55 (kb. 15,9 kHz), 16384 bajt
;            negyszogjel (kb. 440 Hz) a $DF DAC-portra, kb. 1 masodperc
;
; Minden meresnel: a kepsor 0-ra varunk, elinditjuk a DMA-t, a CPU egy
; ciklusban szamol, amig a cel utolso bajtja meg nem valtozik, majd
; kiolvassuk a kepsort. "sor" = eltelt kepsorok, "ciklus" = a CPU hany
; kort tett meg (folyamatos modban a CPU all, ezert ott ~1).
;
; Forditas: sjasmplus dmatest.asm  (kimenet: dmatest.nex)
; =============================================================================

    device zxspectrumnext

DMA_PORT    equ $6B
NR_SELECT   equ $243B

SRC         equ $C000
DST         equ $4000

    org $8000

start:
    di
    ld sp,$BFFE
    nextreg $07,0               ; 3,5 MHz a kiiratashoz
    xor a
    out ($FE),a

    ; tesztterulet: $4000-$7FFF (cel) es $C000-$FFFF (forras) sajat lapokon
    nextreg $56,36
    nextreg $57,37

    ; --- meresek 3,5 MHz-en, majd 28 MHz-en ---
    xor a
    ld (speed_idx),a
    nextreg $07,0
    call run_all
    ld a,1
    ld (speed_idx),a
    nextreg $07,3
    call run_all

    ; --- kiiras ---
    nextreg $07,0
    nextreg $52,10              ; kepernyo vissza
    nextreg $53,11
    call cls
    call print_report

    ; --- E: hangteszt ---
    call sound_test

    ld hl,txt_done
    call print_at_hl_str
.end:
    jr .end

; -----------------------------------------------------------------------------
; Minden meres az aktualis sebesseggel. Eredmenyek: results + speed*24
; -----------------------------------------------------------------------------
run_all:
    nextreg $52,34              ; cel lapok (nem a kepernyo)
    nextreg $53,35
    ; tenyleges sebesseg (NR $07 5-4. bit)
    ld a,$07
    call read_nr
    rrca
    rrca
    rrca
    rrca
    and 3
    ld hl,actual_speed
    ld de,(speed_idx)
    ld d,0
    add hl,de
    ld (hl),a

    ld ix,tests
    ld iy,results
    ld a,(speed_idx)
    or a
    jr z,.r0
    ld de,24
    add iy,de
.r0:
    ld b,6
.loop:
    push bc
    call run_one
    ld (iy+0),l                 ; sorok
    ld (iy+1),h
    ld (iy+2),e                 ; ciklusok
    ld (iy+3),d
    ld de,4
    add iy,de
    ld de,7
    add ix,de
    pop bc
    djnz .loop
    ret

; -----------------------------------------------------------------------------
; Egy meres. IX = leiro: mod(0 folyt,1 loketes), elooszto, ciklusbajt,
; hossz 3,5 MHz-en (dw), hossz 28 MHz-en (dw)
; Kimenet: HL = eltelt sorok, DE = CPU-ciklusok
; -----------------------------------------------------------------------------
run_one:
    ; hossz kivalasztasa
    ld l,(ix+3)
    ld h,(ix+4)
    ld a,(speed_idx)
    or a
    jr z,.len_ok
    ld l,(ix+5)
    ld h,(ix+6)
.len_ok:
    ld (cur_len),hl
    ; jelzobajtok: forras utolso bajtja $AA, cel utolso bajtja 0
    dec hl
    push hl
    ld de,SRC
    add hl,de
    ld (hl),$AA
    pop hl
    ld de,DST
    add hl,de
    ld (hl),0
    ld (dst_last),hl

    ; DMA programozasa
    ld a,$83                    ; tiltas
    out (DMA_PORT),a
    ld a,$C3                    ; reset
    out (DMA_PORT),a
    ld a,$7D                    ; WR0: A->B, A cim + hossz kovetkezik
    out (DMA_PORT),a
    ld hl,SRC
    ld a,l
    out (DMA_PORT),a
    ld a,h
    out (DMA_PORT),a
    ld hl,(cur_len)
    ld a,l
    out (DMA_PORT),a
    ld a,h
    out (DMA_PORT),a
    ld a,$54                    ; WR1: A port memoria, novekvo, idozites kovetkezik
    out (DMA_PORT),a
    ld a,(ix+2)
    out (DMA_PORT),a
    ld a,$50                    ; WR2: B port memoria, novekvo, idozites kovetkezik
    out (DMA_PORT),a
    ld a,(ix+1)
    or a
    jr z,.nopre
    ld a,(ix+2)
    or $20                      ; elooszto kovetkezik
    out (DMA_PORT),a
    ld a,(ix+1)
    out (DMA_PORT),a
    jr .wr4
.nopre:
    ld a,(ix+2)
    out (DMA_PORT),a
.wr4:
    ld a,$AD                    ; WR4: folyamatos, B cim kovetkezik
    bit 0,(ix+0)
    jr z,.cont
    ld a,$CD                    ; WR4: loketes
.cont:
    out (DMA_PORT),a
    ld hl,DST
    ld a,l
    out (DMA_PORT),a
    ld a,h
    out (DMA_PORT),a
    ld a,$82                    ; WR5: megall a vegen
    out (DMA_PORT),a
    ld a,$CF                    ; LOAD
    out (DMA_PORT),a

    ld de,(dst_last)
    call sync_line0
    ld hl,0
    ld a,$87                    ; ENABLE
    out (DMA_PORT),a
.wait:
    inc hl
    ld a,(de)
    or a
    jr nz,.done
    ld a,h                      ; biztonsagi korlat: 65535 kor
    or l
    jr nz,.wait
    dec hl
.done:
    push hl
    call read_line              ; DE = sor
    ex de,hl                    ; HL = sor
    pop de                      ; DE = ciklusok
    ld a,$83
    out (DMA_PORT),a
    ret

; -----------------------------------------------------------------------------
; E: hangteszt. 16384 bajt negyszogjel $C000-tol, loketes mod, elooszto 55,
; port B = I/O $DF (DAC). A CPU kozben szamolja a kepkockakat (a sorszamlalo
; visszaugrasait), hogy a hang hossza is lathato legyen.
; -----------------------------------------------------------------------------
sound_test:
    ld hl,txt_sound
    call print_at_hl_str

    ; negyszogjel: 18 minta $C0, 18 minta $40 (15909/36 = 442 Hz)
    ld hl,SRC
    ld bc,16384
.fill:
    ld a,(sq_cnt)
    inc a
    cp 36
    jr c,.f1
    xor a
.f1:
    ld (sq_cnt),a
    cp 18
    ld a,$C0
    jr c,.f2
    ld a,$40
.f2:
    ld (hl),a
    inc hl
    dec bc
    ld a,b
    or c
    jr nz,.fill
    ld a,$80                    ; utolso minta kozepre, ez a jelzo is
    ld ($FFFF),a

    ld a,$83
    out (DMA_PORT),a
    ld a,$C3
    out (DMA_PORT),a
    ld a,$7D                    ; WR0: A->B, A cim + hossz
    out (DMA_PORT),a
    xor a
    out (DMA_PORT),a            ; $C000
    ld a,$C0
    out (DMA_PORT),a
    xor a
    out (DMA_PORT),a            ; hossz $4000
    ld a,$40
    out (DMA_PORT),a
    ld a,$54                    ; WR1: A memoria, novekvo
    out (DMA_PORT),a
    ld a,1                      ; 3 ciklus
    out (DMA_PORT),a
    ld a,$68                    ; WR2: B I/O, rogzitett cim, idozites kovetkezik
    out (DMA_PORT),a
    ld a,$21                    ; 3 ciklus + elooszto kovetkezik
    out (DMA_PORT),a
    ld a,55
    out (DMA_PORT),a
    ld a,$CD                    ; WR4: loketes, B cim
    out (DMA_PORT),a
    ld a,$DF
    out (DMA_PORT),a
    xor a
    out (DMA_PORT),a
    ld a,$82
    out (DMA_PORT),a
    ld a,$CF
    out (DMA_PORT),a

    ; a vege jelzese: a DMA szamlaloja helyett a kepkockakat szamoljuk addig,
    ; amig a DMA allapota szerint fut (olvasas: $BF, statusz)
    call sync_line0
    ld hl,0
    ld (frames),hl
    ld a,$87
    out (DMA_PORT),a
    ld hl,0
    ld (last_line),hl
.loop:
    call read_line
    ld hl,(last_line)
    ld (last_line),de
    or a
    sbc hl,de                   ; ha az uj sor kisebb: uj kepkocka
    jr c,.same
    jr z,.same
    ld hl,(frames)
    inc hl
    ld (frames),hl
.same:
    ; DMA statusz: RR0 olvasasa ($BF = olvasasi sorozat elinditasa)
    ld a,$BF
    out (DMA_PORT),a
    in a,(DMA_PORT)             ; statusz: 5. bit 0 = blokk vege elerve
    bit 5,a
    jr z,.done
    ld hl,(frames)
    ld a,h
    cp 2                        ; biztonsagi korlat: 512 kepkocka
    jr c,.loop
.done:
    ld a,$83
    out (DMA_PORT),a
    ld hl,txt_frames
    call print_at_hl_str
    ld hl,(frames)
    call print_dec
    ret

; -----------------------------------------------------------------------------
; Segedrutinok
; -----------------------------------------------------------------------------

; A = NextReg szama -> A = erteke
read_nr:
    push bc
    ld bc,NR_SELECT
    out (c),a
    inc b
    in a,(c)
    pop bc
    ret

; DE = aktualis kepsor (NR $1E 0. bit : NR $1F), konzisztensen
read_line:
    push bc
    push hl
.again:
    ld bc,NR_SELECT
    ld a,$1E
    out (c),a
    inc b
    in h,(c)
    dec b
    ld a,$1F
    out (c),a
    inc b
    in e,(c)
    dec b
    ld a,$1E
    out (c),a
    inc b
    in a,(c)
    cp h
    jr nz,.again
    and 1
    ld d,a
    pop hl
    pop bc
    ret

; varakozas a 0. kepsor elejere
sync_line0:
    push de
.s1:
    call read_line
    ld a,d
    or e
    jr z,.s1
.s2:
    call read_line
    ld a,d
    or e
    jr nz,.s2
    pop de
    ret

; kepernyo torlese: fekete tinta feher papiron, feher keret
cls:
    ld hl,$4000
    ld de,$4001
    ld bc,6144
    ld (hl),0
    ldir
    ld hl,$5800
    ld de,$5801
    ld bc,767
    ld (hl),$38
    ldir
    ld a,7
    out ($FE),a
    ret

; kiiras: (cur_row, cur_col) helyre egy karakter (A)
print_char:
    push hl
    push de
    push bc
    sub 32
    ld l,a
    ld h,0
    add hl,hl
    add hl,hl
    add hl,hl
    ld de,font
    add hl,de                   ; HL = karakterkep
    ; kepernyo-cim: $4000 + (sor & $18)<<8 + (sor & 7)<<5 + oszlop
    ld a,(cur_row)
    ld b,a
    and $18
    or $40
    ld d,a
    ld a,b
    and 7
    rrca
    rrca
    rrca
    ld e,a
    ld a,(cur_col)
    add a,e
    ld e,a
    ld b,8
.pc:
    ld a,(hl)
    ld (de),a
    inc hl
    inc d
    djnz .pc
    ld a,(cur_col)
    inc a
    ld (cur_col),a
    pop bc
    pop de
    pop hl
    ret

; HL -> szoveg: elso bajt sor, masodik oszlop, utana a szoveg 0-val zarva
print_at_hl_str:
    ld a,(hl)
    ld (cur_row),a
    inc hl
    ld a,(hl)
    ld (cur_col),a
    inc hl
; HL -> 0-val zart szoveg az aktualis helyre
print_str:
    ld a,(hl)
    or a
    ret z
    call print_char
    inc hl
    jr print_str

; HL decimalisan, vezeto nullak nelkul, 5 helyre jobbra igazitva
print_dec:
    ld b,0                      ; volt-e mar nem nulla jegy
    ld de,10000
    call .digit
    ld de,1000
    call .digit
    ld de,100
    call .digit
    ld de,10
    call .digit
    ld a,l
    add a,'0'
    jp print_char
.digit:
    ld a,'0'-1
.d1:
    inc a
    or a
    sbc hl,de
    jr nc,.d1
    add hl,de
    cp '0'
    jr nz,.nz
    bit 0,b
    jr nz,.nz
    ld a,' '
    jp print_char
.nz:
    ld b,1
    jp print_char

; A (0-99) ket jeggyel
print_dec2:
    ld l,a
    ld h,0
    ld de,10
    ld a,'0'-1
.t:
    inc a
    or a
    sbc hl,de
    jr nc,.t
    add hl,de
    call print_char
    ld a,l
    add a,'0'
    jp print_char

; -----------------------------------------------------------------------------
; Jelentes
; -----------------------------------------------------------------------------
print_report:
    ld hl,txt_title
    call print_at_hl_str
    ; core verzio: NR $01 (fo.al) es NR $0E (alverzio)
    ld hl,txt_core
    call print_at_hl_str
    ld a,$01
    call read_nr
    push af
    rrca
    rrca
    rrca
    rrca
    and 15
    add a,'0'
    call print_char
    ld a,'.'
    call print_char
    pop af
    and 15
    call print_dec2
    ld a,'.'
    call print_char
    ld a,$0E
    call read_nr
    call print_dec2
    ; 50/60 Hz: NR $05 2. bit
    ld a,$05
    call read_nr
    ld hl,txt_50
    bit 2,a
    jr z,.hz
    ld hl,txt_60
.hz:
    call print_str

    ld hl,txt_head1
    call print_at_hl_str
    ld hl,txt_head2
    call print_at_hl_str
    ; tenyleges sebessegek
    ld hl,txt_speed
    call print_at_hl_str
    ld a,(actual_speed)
    call print_speed
    ld a,20
    ld (cur_col),a
    ld a,(actual_speed+1)
    call print_speed

    ; sorok
    ld ix,results
    ld hl,test_names
    ld a,6
    ld (cur_row),a
    ld b,6
.row:
    push bc
    xor a
    ld (cur_col),a
    push hl
    call print_str
    pop hl
    inc hl
    inc hl
    inc hl
    push hl
    ld a,3
    ld (cur_col),a
    ld l,(ix+0)
    ld h,(ix+1)
    call print_dec
    ld l,(ix+2)
    ld h,(ix+3)
    call print_dec
    ld a,15
    ld (cur_col),a
    ld l,(ix+24)
    ld h,(ix+25)
    call print_dec
    ld l,(ix+26)
    ld h,(ix+27)
    call print_dec
    ld de,4
    add ix,de
    ld a,(cur_row)
    inc a
    ld (cur_row),a
    pop hl
    pop bc
    djnz .row

    ld hl,txt_legend
.leg:
    ld a,(hl)
    cp $FF
    ret z
    call print_at_hl_str
    inc hl
    jr .leg

; A = 0..3 -> "3.5" "7" "14" "28"
print_speed:
    ld hl,txt_spd
    add a,a
    add a,a
    ld e,a
    ld d,0
    add hl,de
    jp print_str

; -----------------------------------------------------------------------------
; Adatok
; -----------------------------------------------------------------------------
; leiro: mod, elooszto, ciklusbajt (00=4, 01=3, 10=2), hossz 3,5 MHz, hossz 28 MHz
tests:
    db 0,0,2 : dw 4096,16384        ; A2
    db 0,0,1 : dw 4096,16384        ; A3
    db 0,0,0 : dw 4096,16384        ; A4
    db 0,8,1 : dw 512,512           ; B8
    db 1,8,1 : dw 512,512           ; C8
    db 1,2,1 : dw 4096,4096         ; D2

test_names:
    db "A2",0, "A3",0, "A4",0, "B8",0, "C8",0, "D2",0

txt_spd:
    db "3.5",0, "7",0,0,0, "14",0,0, "28",0,0

txt_title:  db 0,0,"ZX NEXT DMA IDOZITES TESZT v1",0
txt_core:   db 1,0,"core ",0
txt_50:     db "  50Hz",0
txt_60:     db "  60Hz",0
txt_head1:  db 3,0,"   ---3.5MHz--  ---28MHz---",0
txt_head2:  db 4,0,"     sor  kor    sor  kor",0
txt_speed:  db 5,0,"MHz     ",0
txt_legend:
    db 13,0,"A: folyamatos, elooszto nelkul",0
    db 14,0,"   2/3/4 ciklus, 4K ill. 16K",0
    db 15,0,"B: folyamatos, elooszto 8, 512B",0
    db 16,0,"C: loketes, elooszto 8, 512B",0
    db 17,0,"D: loketes, elooszto 2, 4096B",0
    db $FF
txt_sound:  db 19,0,"E: hang, loketes, el.55, 16K",0
txt_frames: db 20,0,"   kepkocka:",0
txt_done:   db 22,0,"Kesz. Fotozd le a kepernyot!",0

speed_idx:      db 0
actual_speed:   db 0,0
cur_len:        dw 0
dst_last:       dw 0
cur_row:        db 0
cur_col:        db 0
sq_cnt:         db 0
frames:         dw 0
last_line:      dw 0

results:        ds 48

    include "font.asm"

code_end:

    savenex open "dmatest.nex",start,$BFFE
    savenex core 3,0,0
    savenex cfg 7
    savenex auto
    savenex close
