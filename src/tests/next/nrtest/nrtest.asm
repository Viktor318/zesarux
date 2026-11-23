; =============================================================================
; ZX Spectrum Next - NextReg visszaolvasas es kepsorszamlalo teszt (v1)
;
; 1. Pillanatkep: mind a 256 NextReg olvasasa (indulaskor es a vegen).
; 2. Kulon esetek: NEXTREG utasitas es a $243B kivalasztas, paletta-iras es
;    az NR $03 7. bitje, vagoablak-index (NR $18/$1C).
; 3. Altalanos teszt: a lista minden regiszterebe 4 minta ($FF, $00, $55,
;    $AA; csak a maszk bitjei valtoznak), mindegyik utan visszaolvasas, majd
;    az eredeti ertek visszairasa es ujraolvasas.
; 4. Idozites (3,5 MHz): a sorszamlalo (NR $1E/$1F) legnagyobb erteke, az
;    ULA-megszakitas utani sor NR $64 = 0 es $40 mellett, 100 kepsor hossza
;    egy 37 T-allapotos ciklus koreiben (3,5 es 28 MHz).
;
; Az eredmeny a $A000-tol all (RES_LEN bajt); az S billentyure NRTEST.BIN
; neven a kartyara menti (esxDOS API, NextZXOS alol inditva).
;
; Forditas: sjasmplus nrtest.asm  (kimenet: nrtest.nex)
; =============================================================================

    device zxspectrumnext

NR_SELECT   equ $243B

RES         equ $A000
RES_LEN     equ $0700
R_FLAGS     equ RES+$04         ; 1 = kesz
R_COUNT     equ RES+$05         ; altalanos tesztek szama
R_MAXLINE   equ RES+$06         ; dw: legnagyobb sorszam (3,5 MHz)
R_INT0      equ RES+$08         ; dw: sor a megszakitas utan, NR $64 = 0
R_INT64     equ RES+$0A         ; dw: ugyanez NR $64 = $40 mellett
R_P100_3    equ RES+$0C         ; dw: korok 100 sor alatt, 3,5 MHz
R_P100_28   equ RES+$0E         ; dw: ugyanez 28 MHz-en
R_PAL       equ RES+$10         ; 8 bajt: paletta / NR $03 teszt
R_CLIP      equ RES+$18         ; 5 bajt: vagoablak-teszt
R_INT0B     equ RES+$1E         ; dw: megszakitas utani sor ujra NR $64 = 0
R_SEL       equ RES+$20         ; 2 bajt: NEXTREG es a kivalasztas
R_SNAP0     equ RES+$040        ; 256 bajt
R_GEN       equ RES+$140        ; 8 bajt/teszt: reg, maszk, eredeti, FF, 00, 55, AA, vissza
R_SNAP1     equ RES+$600        ; 256 bajt

IM2_TAB     equ $BC00
IM2_HND     equ $BEBE

    org $8000

start:
    di
    ld sp,$BFFE
    nextreg $07,0

    ld hl,RES
    ld de,RES+1
    ld bc,RES_LEN-1
    ld (hl),0
    ldir
    ld hl,magic
    ld de,RES
    ld bc,4
    ldir

    ld hl,R_SNAP0
    call snapshot

    call test_select
    call test_palette
    call test_clip
    call gen_tests
    ; a megszakitas-allapotok torlese (az NR $20 iras utan)
    nextreg $C8,$FF
    nextreg $C9,$FF
    nextreg $CA,$FF
    call timing_tests

    ld hl,R_SNAP1
    call snapshot
    ld a,1
    ld (R_FLAGS),a

    call cls
    call print_summary
.wait_s:
    ld bc,$FDFE                 ; A S D F G sor
    in a,(c)
    bit 1,a
    jr nz,.wait_s
    call save_file
.end:
    jr .end

; -----------------------------------------------------------------------------
; HL = cel; mind a 256 regiszter
; -----------------------------------------------------------------------------
snapshot:
    ld e,0
.l:
    ld a,e
    call read_nr
    ld (hl),a
    inc hl
    inc e
    jr nz,.l
    ret

; -----------------------------------------------------------------------------
; NEXTREG utasitas: nem valtoztathatja a $243B-ben kivalasztott regisztert
; R_SEL+0: a $7F olvasasa $253B-n a NEXTREG $4A utan (vart: $5A)
; R_SEL+1: a $243B olvasasa
; -----------------------------------------------------------------------------
test_select:
    ld bc,NR_SELECT
    ld a,$7F
    out (c),a
    inc b
    ld a,$5A
    out (c),a
    nextreg $4A,$12
    in a,(c)
    ld (R_SEL),a
    dec b
    in a,(c)
    ld (R_SEL+1),a
    ld a,(R_SNAP0+$4A)
    nextreg $4A,a
    ld a,(R_SNAP0+$7F)
    nextreg $7F,a
    ret

; -----------------------------------------------------------------------------
; Paletta es NR $03 7. bit (9 bites iras elso fele).
; R_PAL: +0 NR $41 ($FE), +1 NR $44 ($FE), +2 NR $03 elotte, +3 NR $03 az
; elso $44 iras utan, +4 NR $03 a masodik utan, +5 NR $40 utana,
; +6 NR $41 a $E3 iras utan, +7 NR $44 ugyanott
; -----------------------------------------------------------------------------
test_palette:
    nextreg $43,$00
    nextreg $40,$FE
    ld a,$41
    call read_nr
    ld (R_PAL+0),a
    ld a,$44
    call read_nr
    ld (R_PAL+1),a
    ld a,$03
    call read_nr
    ld (R_PAL+2),a
    nextreg $40,$FE
    ld a,(R_PAL+0)
    nextreg $44,a
    ld a,$03
    call read_nr
    ld (R_PAL+3),a
    ld a,(R_PAL+1)
    nextreg $44,a
    ld a,$03
    call read_nr
    ld (R_PAL+4),a
    ld a,$40
    call read_nr
    ld (R_PAL+5),a
    nextreg $40,$FE
    nextreg $41,$E3
    nextreg $40,$FE
    ld a,$41
    call read_nr
    ld (R_PAL+6),a
    ld a,$44
    call read_nr
    ld (R_PAL+7),a
    ; vissza
    nextreg $40,$FE
    ld a,(R_PAL+0)
    nextreg $44,a
    ld a,(R_PAL+1)
    nextreg $44,a
    ld a,(R_SNAP0+$40)
    nextreg $40,a
    ld a,(R_SNAP0+$43)
    nextreg $43,a
    ret

; -----------------------------------------------------------------------------
; Vagoablak (Layer 2): az olvasas nem lepteti az indexet, az iras igen.
; R_CLIP: +0 NR $1C a torles utan, +1 NR $18 harom iras utan, +2 NR $1C
; ugyanott, +3 NR $18 a negyedik iras utan, +4 NR $1C ugyanott
; -----------------------------------------------------------------------------
test_clip:
    nextreg $1C,$0F
    ld a,$1C
    call read_nr
    ld (R_CLIP+0),a
    nextreg $18,$11
    nextreg $18,$22
    nextreg $18,$33
    ld a,$18
    call read_nr
    ld (R_CLIP+1),a
    ld a,$1C
    call read_nr
    ld (R_CLIP+2),a
    nextreg $18,$44
    ld a,$18
    call read_nr
    ld (R_CLIP+3),a
    ld a,$1C
    call read_nr
    ld (R_CLIP+4),a
    ; alapertek: 0,255,0,191
    nextreg $1C,$01
    nextreg $18,0
    nextreg $18,255
    nextreg $18,0
    nextreg $18,191
    ret

; -----------------------------------------------------------------------------
; Altalanos iras/olvasas teszt
; -----------------------------------------------------------------------------
gen_tests:
    ld ix,gen_list
    ld hl,R_GEN
    ld b,GEN_COUNT
    ld a,b
    ld (R_COUNT),a
.loop:
    push bc
    ld a,(ix+0)
    ld (hl),a
    inc hl
    ld a,(ix+1)
    ld (hl),a
    inc hl
    ld a,(ix+0)
    call read_nr
    ld (hl),a
    inc hl
    ld (cur_orig),a
    ld a,$FF
    call gen_one
    xor a
    call gen_one
    ld a,$55
    call gen_one
    ld a,$AA
    call gen_one
    ld a,(cur_orig)
    ld e,a
    ld a,(ix+0)
    call write_nr
    ld a,(ix+0)
    call read_nr
    ld (hl),a
    inc hl
    inc ix
    inc ix
    pop bc
    djnz .loop
    ret

; A = minta; irt ertek = (eredeti AND NOT maszk) OR (minta AND maszk)
gen_one:
    ld d,a
    ld a,(ix+1)
    and d
    ld d,a
    ld a,(ix+1)
    cpl
    ld e,a
    ld a,(cur_orig)
    and e
    or d
    ld e,a
    ld a,(ix+0)
    call write_nr
    ld a,(ix+0)
    call read_nr
    ld (hl),a
    inc hl
    ret

; regiszter, maszk (a maszkon kivuli bitek az eredeti erteket kapjak)
gen_list:
    db $05,$FA                  ; 50/60 Hz (2.) es scandoubler (0.) marad
    db $06,$FF
    db $07,$03
    db $08,$FF
    db $09,$F7                  ; 3. bit (DivMMC mapram torlese) kimarad
    db $0A,$FF
    db $0B,$FF
    db $12,$FF
    db $13,$FF
    db $14,$FF
    db $15,$FF
    db $16,$FF
    db $17,$FF
    db $22,$FF
    db $23,$FF
    db $26,$FF
    db $27,$FF
    db $2F,$FF
    db $30,$FF
    db $31,$FF
    db $32,$FF
    db $40,$FF
    db $42,$FF
    db $43,$FF
    db $4A,$FF
    db $4B,$FF
    db $4C,$FF
    db $50,$FF
    db $51,$FF
    db $52,$FF
    db $53,$FF
    db $56,$FF
    db $57,$FF
    db $61,$FF
    db $62,$07                  ; a copper uzemmodja (7-6.) marad
    db $64,$FF
    db $68,$FF
    db $69,$FF
    db $6A,$FF
    db $6B,$FF
    db $6C,$FF
    db $6E,$FF
    db $6F,$FF
    db $70,$FF
    db $71,$FF
    db $7F,$FF
    db $82,$FF
    db $83,$FF
    db $84,$FF
    db $85,$FF
    db $86,$FF
    db $87,$FF
    db $88,$FF
    db $89,$FF
    db $8A,$FF
    db $8C,$FF
    db $8E,$FB                  ; kulonleges (csupa RAM) mod kimarad
    db $8F,$FF
    db $B8,$FF
    db $B9,$FF
    db $BA,$FF
    db $BB,$FF
    db $C0,$FF
    db $C4,$FF
    db $C5,$FF
    db $C6,$FF
    db $CC,$FF
    db $CD,$FF
    db $CE,$FF
    db $D8,$FF
    db $D9,$FF
    db $20,$FF
    db $C8,$FF
    db $C9,$FF
    db $CA,$FF
GEN_COUNT   equ ($-gen_list)/2

; -----------------------------------------------------------------------------
; Idozites
; -----------------------------------------------------------------------------
timing_tests:
    nextreg $07,0
    ; legnagyobb sorszam (kb. 6 kepkocka)
    ld hl,0
    ld bc,1500
.m:
    push bc
    call read_line
    push hl
    or a
    sbc hl,de
    pop hl
    jr nc,.nm
    ex de,hl
.nm:
    pop bc
    dec bc
    ld a,b
    or c
    jr nz,.m
    ld (R_MAXLINE),hl

    ; IM2 a sajat tablaval: EI, RETI
    ld hl,IM2_TAB
    ld de,IM2_TAB+1
    ld bc,256
    ld (hl),IM2_HND & $FF
    ldir
    ld a,$FB
    ld (IM2_HND),a
    ld a,$ED
    ld (IM2_HND+1),a
    ld a,$4D
    ld (IM2_HND+2),a
    ld a,IM2_TAB>>8
    ld i,a
    im 2
    ld a,$C0
    call read_nr
    and $E8                     ; impulzus mod
    nextreg $C0,a
    nextreg $22,0               ; ULA-megszakitas be, sor-megszakitas ki
    nextreg $64,0
    ei
    halt
    halt
    call read_line
    ld (R_INT0),de
    nextreg $64,$40
    halt
    halt
    call read_line
    ld (R_INT64),de
    nextreg $64,0
    halt
    halt
    call read_line
    ld (R_INT0B),de
    di
    ld a,(R_SNAP0+$22)
    and $07
    nextreg $22,a
    ld a,(R_SNAP0+$C0)
    nextreg $C0,a
    ld a,(R_SNAP0+$64)
    nextreg $64,a

    ; 100 sor hossza
    call measure100
    ld (R_P100_3),de
    nextreg $07,3
    call measure100
    ld (R_P100_28),de
    nextreg $07,0
    ret

; DE = a 37 T-allapotos ciklus koreinek szama a 20. es a 120. sor kozott
measure100:
    call sync_line0
    ld bc,NR_SELECT
    ld a,$1F
    out (c),a
    inc b
.w:
    in a,(c)
    cp 20
    jr nz,.w
    ld de,0
.c:
    inc de
    in a,(c)
    cp 120
    jr nz,.c
    ret

; -----------------------------------------------------------------------------
; Mentes: NRTEST.BIN (esxDOS API)
; -----------------------------------------------------------------------------
save_file:
    ld hl,txt_saving
    call print_at_hl_str
    ld a,'*'
    ld ix,fname
    ld b,$0E                    ; iras, letrehozas/felulirasa
    rst $08
    db $9A                      ; F_OPEN
    jr c,.err
    ld (handle),a
    ld ix,RES
    ld bc,RES_LEN
    rst $08
    db $9E                      ; F_WRITE
    jr c,.err
    ld a,(handle)
    rst $08
    db $9B                      ; F_CLOSE
    jr c,.err
    ld hl,txt_saved
    jp print_at_hl_str
.err:
    push af
    ld hl,txt_error
    call print_at_hl_str
    pop af
    jp print_hex8

; -----------------------------------------------------------------------------
; Osszefoglalo
; -----------------------------------------------------------------------------
print_summary:
    ld hl,txt_title
    call print_at_hl_str
    ld hl,txt_core
    call print_at_hl_str
    ld a,(R_SNAP0+$01)
    push af
    rrca
    rrca
    rrca
    rrca
    and $0F
    add a,'0'
    call print_char
    ld a,'.'
    call print_char
    pop af
    and $0F
    call print_dec2
    ld a,'.'
    call print_char
    ld a,(R_SNAP0+$0E)
    call print_dec2

    ld hl,txt_regs
    call print_at_hl_str
    ld a,(R_SNAP0+$03)
    call print_hex8
    ld hl,txt_nr05
    call print_str
    ld a,(R_SNAP0+$05)
    call print_hex8
    ld hl,txt_nr08
    call print_str
    ld a,(R_SNAP0+$08)
    call print_hex8

    ld hl,txt_lines
    call print_at_hl_str
    ld hl,(R_MAXLINE)
    inc hl
    call print_dec
    ld hl,txt_int0
    call print_at_hl_str
    ld hl,(R_INT0)
    call print_dec
    ld hl,txt_int64
    call print_at_hl_str
    ld hl,(R_INT64)
    call print_dec
    ld hl,txt_int0b
    call print_at_hl_str
    ld hl,(R_INT0B)
    call print_dec
    ld hl,txt_p100
    call print_at_hl_str
    ld hl,(R_P100_3)
    call print_dec
    ld hl,txt_slash
    call print_str
    ld hl,(R_P100_28)
    call print_dec

    ld hl,txt_count
    call print_at_hl_str
    ld a,(R_COUNT)
    ld l,a
    ld h,0
    call print_dec
    ld hl,txt_sum
    call print_str
    ; 16 bites osszeg a teljes eredmenyterulet bajtjaibol
    ld hl,RES
    ld bc,RES_LEN
    ld de,0
.s:
    ld a,(hl)
    add a,e
    ld e,a
    jr nc,.s1
    inc d
.s1:
    inc hl
    dec bc
    ld a,b
    or c
    jr nz,.s
    ld a,d
    call print_hex8
    ld a,e
    call print_hex8

    ld hl,txt_press
    jp print_at_hl_str

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

; A = NextReg szama, E = ertek
write_nr:
    push bc
    ld bc,NR_SELECT
    out (c),a
    inc b
    out (c),e
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
    add hl,de
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
print_str:
    ld a,(hl)
    or a
    ret z
    call print_char
    inc hl
    jr print_str

; HL decimalisan, 5 helyre jobbra igazitva
print_dec:
    ld b,0
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

; A hexadecimalisan
print_hex8:
    push af
    rrca
    rrca
    rrca
    rrca
    call .nib
    pop af
.nib:
    and $0F
    add a,'0'
    cp '9'+1
    jr c,.p
    add a,7
.p:
    jp print_char

; -----------------------------------------------------------------------------
magic:      db "NRT1"
fname:      db "NRTEST.BIN",0
txt_title:  db 0,0,"NEXTREG TESZT v1",0
txt_core:   db 1,0,"core ",0
txt_regs:   db 2,0,"NR03=",0
txt_nr05:   db " NR05=",0
txt_nr08:   db " NR08=",0
txt_lines:  db 4,0,"sorok/kepkocka:   ",0
txt_int0:   db 5,0,"sor az INT utan:  ",0
txt_int64:  db 6,0,"  NR64=40 mellett:",0
txt_int0b:  db 7,0,"  NR64=0 ujra:    ",0
txt_p100:   db 9,0,"100 sor, kor:",0
txt_slash:  db " /",0
txt_count:  db 11,0,"regiszterek:",0
txt_sum:    db "  osszeg: ",0
txt_press:  db 13,0,"S = mentes: NRTEST.BIN",0
txt_saving: db 15,0,"Mentes...",0
txt_saved:  db 15,0,"Mentve: NRTEST.BIN   ",0
txt_error:  db 15,0,"Hiba a mentesnel: ",0

cur_row:    db 0
cur_col:    db 0
cur_orig:   db 0
handle:     db 0

    include "../dmatest/font.asm"

code_end:
    assert code_end < RES, "a kod tul hosszu"

    savenex open "nrtest.nex",start,$BFFE
    savenex core 3,0,0
    savenex cfg 7
    savenex auto
    savenex close
