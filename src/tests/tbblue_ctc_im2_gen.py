#!/usr/bin/env python3
# Tesztprogramok a CTC-hez es az IM2-hoz: forgatokonyvenkent egy asm
# Hasznalat: python3 tbblue_ctc_im2_gen.py  -> s1.asm..s7.asm; forditas: sjasmplus s1.asm --raw=s1.bin
# A lefordított bájtok a tbblue_ctc_im2.py PROGRAMOK táblájába kerülnek.
import sys
COMMON_HEAD = r"""
 device zxspectrumnext
 org 8000h
start:
 di
 ld sp,0BF00h
 nextreg 07h,0
 ld hl,0A000h
 ld b,32
clr: ld (hl),0
 inc hl
 djnz clr
 ld a,90h
 ld i,a
 im 2
 ; vektortabla: pulzus mod (busz=FFh) -> 90FFh, hw IM2 (C0 alap A0h): vonal A0h, ctc0 A6h, ctc1 A8h, ula B6h
 ld hl,h_pulse
 ld (90FFh),hl
 ld hl,h_line
 ld (90A0h),hl
 ld hl,h_ctc0
 ld (90A6h),hl
 ld hl,h_ctc1
 ld (90A8h),hl
 ld hl,h_ula
 ld (90B6h),hl
"""
COMMON_TAIL = r"""
delay:
 ld c,64
outer: ld b,0
inner: djnz inner
 dec c
 jr nz,outer
 ret

nr_read: ; A = regiszter -> A = ertek
 push bc
 ld bc,243Bh
 out (c),a
 inc b
 in a,(c)
 pop bc
 ret

done:
 di
 ld a,0C0h
 call nr_read
 ld (0A010h),a
 ld a,0C5h
 call nr_read
 ld (0A011h),a
 ld a,0C8h
 call nr_read
 ld (0A012h),a
 ld a,0C9h
 call nr_read
 ld (0A013h),a
 ld a,20h
 call nr_read
 ld (0A014h),a
 ld bc,183Bh
 in a,(c)
 ld (0A015h),a
 ld a,55h
 ld (0A01Fh),a
 jr $

COUNT macro addr
 push hl
 ld hl,(addr)
 inc hl
 ld (addr),hl
 pop hl
 endm

h_pulse: COUNT 0A000h
 ei
 reti
h_ctc0: COUNT 0A002h
 ei
 RETCTC
h_ctc1: COUNT 0A004h
 ei
 reti
h_ula: COUNT 0A006h
 ei
 reti
h_line: COUNT 0A008h
 ei
 reti
 savebin "{name}.bin",start,$-start
"""
SCEN = {
# 1. Pulzus mod, CTC0 idozito megszakitassal, ULA tiltva: a CTC a szokasos /INT pulzust adja
"s1": """
 nextreg 22h,04h
 nextreg 0C0h,0
 ld bc,183Bh
 ld a,0A5h
 out (c),a
 ld a,100
 out (c),a
 ei
 call delay
 jp done
""",
# 2. HW IM2, CTC0 + ULA, sajat vektorok
"s2": """
 nextreg 22h,0
 nextreg 0C0h,0A1h
 ld bc,183Bh
 ld a,0A5h
 out (c),a
 ld a,100
 out (c),a
 ei
 call delay
 jp done
""",
# 3. Mint a 2., de a CTC0 kezeloje RET-tel ter vissza (nincs RETI): a CTC0 szolgalatban marad, es blokkolja az alacsonyabb prioritasu ULA-t is
"s3": """
 nextreg 22h,0
 nextreg 0C0h,0A1h
 ld bc,183Bh
 ld a,0A5h
 out (c),a
 ld a,100
 out (c),a
 ei
 call delay
 jp done
""",
# 4. HW IM2, lancolas: CTC0 idozito (16-os eloszto, TC=50, megszakitas nelkul) -> CTC1 szamlalo (TC=5, megszakitassal)
"s4": """
 nextreg 22h,04h
 nextreg 0C0h,0A1h
 ld bc,183Bh
 ld a,05h
 out (c),a
 ld a,50
 out (c),a
 ld bc,193Bh
 ld a,0C5h
 out (c),a
 ld a,5
 out (c),a
 ei
 call delay
 jp done
""",
# 5. HW IM2, vonalmegszakitas (100. sor) + ULA
"s5": """
 nextreg 23h,100
 nextreg 22h,02h
 nextreg 0C0h,0A1h
 ei
 call delay
 jp done
""",
# 6. HW IM2, NR 0x20: feltetel nelkuli CTC0-megszakitas (a csatorna nem fut, nincs engedelyezve)
"s6": """
 nextreg 22h,04h
 nextreg 0C0h,0A1h
 nextreg 0C9h,0FFh
 nextreg 20h,01h
 ei
 call delay
 jp done
""",
# 7. HW IM2, ULA tiltva a C4 0. bitjevel: nincs ULA-megszakitas, de az allapotbit beall
"s7": """
 nextreg 0C8h,0FFh
 nextreg 0C4h,0
 nextreg 0C0h,0A1h
 ei
 call delay
 jp done
""",
}
for name,body in SCEN.items():
    tail = COMMON_TAIL.replace("{name}",name).replace("RETCTC", "ret" if name=="s3" else "reti")
    open(name+".asm","w").write(COMMON_HEAD+body+tail)
