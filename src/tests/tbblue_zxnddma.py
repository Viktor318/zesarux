#!/usr/bin/env python3
"""zxnDDMA-tesztek a ZEsarUX-on ZRCP-n át: a DMA 0x6B portjára ír, a Next fizikai RAM-ját olvassa vissza.
   Indítás az src mappából: python3 tests/tbblue_zxnddma.py [zesarux mappa, alap: .]"""
import socket,subprocess,sys,time,os
PORT=10007
class Z:
    def __init__(s,bindir):
        s.p=subprocess.Popen(["./zesarux","--noconfigfile","--vo","null","--ao","null","--machine","tbblue",
            "--tbblue-fast-boot-mode","--enable-remoteprotocol","--remoteprotocol-port",str(PORT),"--exit-after","120"],
            cwd=bindir,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        for i in range(40):
            try: s.s=socket.create_connection(("127.0.0.1",PORT),timeout=5); break
            except OSError: time.sleep(0.25)
        s.rd()
        # a CPU áll: csak a DMA dolgozik, amikor léptetünk
        s.c("enter-cpu-step")
    def rd(s):
        b=b""
        while not (b.rstrip().endswith(b"command>") or b.rstrip().endswith(b"command@cpu-step>")):
            x=s.s.recv(65536)
            if not x: break
            b+=x
        t=b.decode(errors="replace")
        return t.rsplit("\n",1)[0].strip() if "\n" in t else ""
    def c(s,cmd):
        s.s.sendall((cmd+"\r\n").encode()); return s.rd()
    def zone(s,z): s.c(f"set-memory-zone {z}")
    def wr(s,addr,data,zone=0):
        s.zone(zone)
        for i in range(0,len(data),64):
            s.c(f"write-memory {addr+i} "+" ".join(str(b) for b in data[i:i+64]))
    def rdm(s,addr,n,zone=0):
        s.zone(zone)
        return bytes.fromhex(s.c(f"read-memory {addr} {n}"))
    def reg(s,r,v): s.c(f"tbblue-set-register {r} {v}")
    def dma(s,*vals,port=0x6B):
        for v in vals: s.c(f"write-port {port} {v}")
    def run(s,n=4):
        # néhány utasítás: a mag minden utasítás után meghívja a DMA-t
        s.c(f"cpu-step")
        for i in range(n-1): s.c("cpu-step")
    def close(s):
        try: s.c("exit-emulator")
        except Exception: pass
        s.p.wait(timeout=10)

# A zóna 0 (Machine RAM) a Next teljes fizikai címtere (ROM-ok is): az eltolás maga a fizikai cím
RAM=0
ok=fail=0
def check(name,got,exp):
    global ok,fail
    if got==exp: ok+=1; print("  OK  ",name)
    else:
        fail+=1; print("  HIBA",name); print("     kapott:",got.hex() if isinstance(got,bytes) else got); print("     várt:  ",exp.hex() if isinstance(exp,bytes) else exp)

z=Z(sys.argv[1] if len(sys.argv)>1 else ".")
P=lambda phys: phys-RAM          # fizikai cím -> zóna 0 eltolás
lo=lambda v: v&255
hi=lambda v: (v>>8)&255

# --- 1. A régi (16 bites) másolás változatlan: MMU-n át, 0xC000 -> 0xC100 -------------------------
src=bytes(range(1,33))
z.wr(0xC000,list(src),zone=-1); z.wr(0xC100,[0]*40,zone=-1)
z.dma(0xC3, 0x7D,0x00,0xC0,32,0, 0x14, 0x10, 0xAD,0x00,0xC1, 0x82, 0xCF, 0x87); z.run()
check("1. sima zxnDMA-másolás (MMU-címek)", z.rdm(0xC100,34,zone=-1), src+b"\0\0")

# --- 2. Fizikai cím: 0x0A0000 -> 0x0B0000, 16 bájt -------------------------------------------------
src=bytes(range(0x40,0x50))
z.wr(P(0x0A0000),list(src)); z.wr(P(0x0B0000),[0xEE]*20)
z.dma(0xC3, 0xD7,0x0A, 0xDB,0x0B, 0xDF,0x01, 0x7D,0,0,16,0, 0x14, 0x10, 0xAD,0,0, 0x82, 0xCF, 0x87); z.run()
check("2. fizikai címek ($D7/$DB, $DF bit 0)", z.rdm(P(0x0B0000),18), src+b"\xEE\xEE")

# --- 3. Fizikai mód: átvitel a 64K határon (0x0AFFFC -> 0x0C0000 felfelé) --------------------------
src=bytes(range(0x10,0x18))
z.wr(P(0x0AFFFC),list(src)); z.wr(P(0x0CFFFE),[0]*12)
z.dma(0xC3, 0xD7,0x0A, 0xDB,0x0C, 0xDF,0x01, 0x7D,0xFC,0xFF,8,0, 0x14, 0x10, 0xAD,0xFE,0xFF, 0x82, 0xCF, 0x87); z.run()
check("3. átvitel a 64K-s határon (mindkét oldalon)", z.rdm(P(0x0CFFFE),10), src+b"\0\0")

# --- 4. Téglalap: 8x4 forrás (lépés 16) -> cél (lépés 32) ------------------------------------------
sheet=[ (r*16+c) for r in range(6) for c in range(16)]
z.wr(P(0x0A1000),sheet); z.wr(P(0x0B1000),[0]*(32*6))
z.dma(0xC3, 0xD7,0x0A, 0xDB,0x0B, 0xDF,0x03, 0xE3,8,0, 0xE7,4,0, 0xEB,16,0, 0xEF,32,0,
      0x7D,0x02,0x10,0,0, 0x14, 0x10, 0xAD,0x05,0x10, 0x82, 0xCF, 0x87); z.run()
exp=bytearray(32*6)
for r in range(4):
    for c in range(8): exp[r*32+5+c]=sheet[r*16+2+c]
check("4. téglalap (sorhossz, sorok, lépések)", z.rdm(P(0x0B1000),32*6), bytes(exp))

# --- 5. Átlátszó szín: a 0xE3 bájtok nem íródnak ---------------------------------------------------
src=[1,0xE3,2,0xE3,0xE3,3,4,0xE3]
z.wr(P(0x0A2000),src); z.wr(P(0x0B2000),[0x77]*10)
z.dma(0xC3, 0xD7,0x0A, 0xDB,0x0B, 0xDF,0x05, 0xF3,0xE3, 0x7D,0,0x20,8,0, 0x14, 0x10, 0xAD,0,0x20, 0x82, 0xCF, 0x87); z.run()
check("5. átlátszó szín ($F3, $DF bit 2)", z.rdm(P(0x0B2000),10), bytes([1,0x77,2,0x77,0x77,3,4,0x77,0x77,0x77]))

# --- 6. Kitöltés téglalappal: 5x3, érték 0x5A ------------------------------------------------------
z.wr(P(0x0B3000),[0]*64)
z.dma(0xC3, 0xDB,0x0B, 0xDF,0x0B, 0xE3,5,0, 0xE7,3,0, 0xEF,16,0, 0xF7,0x5A,
      0x7D,0,0,0,0, 0x14, 0x10, 0xAD,0x01,0x30, 0x82, 0xCF, 0x87); z.run()
exp=bytearray(64)
for r in range(3):
    for c in range(5): exp[r*16+1+c]=0x5A
check("6. kitöltés ($F7, $DF bit 3) téglalapban", z.rdm(P(0x0B3000),64), bytes(exp))

# --- 7. A $C3 törli a módokat: utána sima MMU-másolás, nem kitöltés --------------------------------
src=bytes([9,8,7,6])
z.wr(0xC200,list(src),zone=-1); z.wr(0xC300,[0]*6,zone=-1)
z.dma(0xC3, 0x7D,0x00,0xC2,4,0, 0x14, 0x10, 0xAD,0x00,0xC3, 0x82, 0xCF, 0x87); z.run()
check("7. $C3 után nincs kitöltés/fizikai mód", z.rdm(0xC300,6,zone=-1), src+b"\0\0")

# --- 8. Layer 2 X,Y, 256x192: bank a 0x12-es regiszterből -------------------------------------------
z.reg(0x12,9); z.reg(0x13,12); z.reg(0x70,0x00)
base=0x40000+9*0x4000
sprite=[ (0x80+r*4+c) for r in range(3) for c in range(4)]
z.wr(P(0x0A4000),sprite); z.wr(P(base+20*256+96),[0]*(256*3))
z.dma(0xC3, 0xD7,0x0A, 0xE3,4,0, 0xE7,3,0, 0xEB,4,0, 0x7D,0,0x40,0,0, 0x14, 0x10, 0xA1,
      0xDF,0x13,100,0,20, 0x82, 0xCF, 0x87); z.run()
got=b"".join(z.rdm(P(base+(20+r)*256+100),4) for r in range(3))
check("8. Layer 2 X,Y 256x192 (sorfolytonos)", got, bytes(sprite))

# --- 9. Layer 2 X,Y, 320x256 (oszlopfolytonos), rejtett képernyő (0x13) -----------------------------
z.reg(0x70,0x10)
base=0x40000+12*0x4000
z.wr(P(base+300*256+200),[0]*(256*3))
z.dma(0xC3, 0xD7,0x0A, 0xE3,4,0, 0xE7,3,0, 0xEB,4,0, 0x7D,0,0x40,0,0, 0x14, 0x10, 0xA1,
      0xDF,0x33,lo(300),hi(300),200, 0x82, 0xCF, 0x87); z.run()
got=b"".join(z.rdm(P(base+(300+r)*256+200),4) for r in range(3))
check("9. Layer 2 X,Y 320x256, rejtett képernyő ($DF bit 5)", got, bytes(sprite))

# --- 10. Rövid újratöltés: $1D lo hi, $DF ... x y, $CF $87 (a $D7 megmarad) -------------------------
z.dma(0x1D,0,0x40, 0xDF,0x33,lo(10),hi(10),50, 0xCF, 0x87); z.run()
got=b"".join(z.rdm(P(base+(10+r)*256+50),4) for r in range(3))
check("10. rövid újratöltés ugyanazzal az alakkal", got, bytes(sprite))

# --- 11. A 0x0B porton a DDMA-parancs elvész, a követő bájt regiszterírás ---------------------------
#  $DF $01 a 0x0B-n: a $01 WR0-ként értelmeződik (követő bájt nélkül), a fizikai mód NEM kapcsol be
src=bytes([0x21,0x22,0x23,0x24])
z.wr(0xC400,list(src),zone=-1); z.wr(0xC500,[0]*6,zone=-1); z.wr(P(0x0BC500),[0x99]*6)
z.dma(0xC3, 0xDB,0x0B, 0xDF,0x01, port=0x0B)
z.dma(0x7D,0x00,0xC4,4,0, 0x14, 0x10, 0xAD,0x00,0xC5, 0x82, 0xCF, 0x87, port=0x0B); z.run()
check("11a. 0x0B port: MMU-cím szerint másol", z.rdm(0xC500,6,zone=-1), src+b"\0\0")
check("11b. 0x0B port: a fizikai 0x0BC500 érintetlen", z.rdm(P(0x0BC500),6), b"\x99"*6)

# --- 12. Lefelé számláló cím téglalapban (tükrözés): forrás fel, cél le ----------------------------
src=[1,2,3,4, 5,6,7,8]
z.wr(P(0x0A5000),src); z.wr(P(0x0B5000),[0]*32)
z.dma(0xC3, 0xD7,0x0A, 0xDB,0x0B, 0xDF,0x03, 0xE3,4,0, 0xE7,2,0, 0xEB,4,0, 0xEF,16,0,
      0x7D,0,0x50,0,0, 0x14, 0x00, 0xAD,0x03,0x50, 0x82, 0xCF, 0x87); z.run()
exp=bytearray(32); exp[0:4]=bytes([4,3,2,1]); exp[16:20]=bytes([8,7,6,5])
check("12. téglalap lefelé számláló célcímmel (tükrözés)", z.rdm(P(0x0B5000),32), bytes(exp))

# --- 13. Ismétlés csak $DF x y + $CF $87-tel: a $CF a forrás kezdőcímét is visszatölti --------------
#  (a zxnddma_compare példa így rajzol: a forráscímet csak egyszer küldi el)
z.reg(0x70,0x10); base=0x40000+9*0x4000
z.wr(P(base+40*256+10),[0]*8); z.wr(P(base+60*256+10),[0]*8)
z.dma(0xC3, 0x14, 0x10, 0xA1, 0x82, 0x1D,0,0x40, 0xD7,0x0A, 0xE3,4,0, 0xE7,1,0, 0xEB,4,0)
z.dma(0xDF,0x13,40,0,10, 0xCF, 0x87); z.run()
z.dma(0xDF,0x13,60,0,10, 0xCF, 0x87); z.run()
check("13a. első rajzolás", z.rdm(P(base+40*256+10),4), bytes(sprite[:4]))
check("13b. ismétlés $1D nélkül: ugyanaz a forrás", z.rdm(P(base+60*256+10),4), bytes(sprite[:4]))

# --- 14. Ugyanez a régi (16 bites) DMA-val: $CF $87 újra ugyanazt másolja ---------------------------
src=bytes([0x31,0x32,0x33,0x34])
z.wr(0xC600,list(src)+[0xAA]*4,zone=-1); z.wr(0xC700,[0]*8,zone=-1)
z.dma(0xC3, 0x7D,0x00,0xC6,4,0, 0x14, 0x10, 0xAD,0x00,0xC7, 0x82, 0xCF, 0x87); z.run()
z.wr(0xC700,[0]*8,zone=-1)
z.dma(0xCF, 0x87); z.run()
check("14. sima DMA: $CF visszatölti a kezdőcímeket", z.rdm(0xC700,8,zone=-1), src+b"\0\0\0\0")

z.close()
print(f"\n{ok} rendben, {fail} hibás")
sys.exit(1 if fail else 0)
