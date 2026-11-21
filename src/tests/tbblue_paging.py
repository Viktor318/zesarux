#!/usr/bin/env python3
"""A Next klasszikus lapozásának tesztje ZRCP-n át: 7FFD-zár és NR 0x08 bit 7, DFFD, NR 0x8E írása és
   olvasása, a +3 „special” (csupa RAM) mód be- és kikapcsolása (zxnext.vhd szerint, a JNEXT mmu.cpp-je alapján).
   Indítás az src mappából: python3 tests/tbblue_paging.py [zesarux mappa, alap: .]"""
import socket,subprocess,sys,time
PORT=10008
p=subprocess.Popen(["./zesarux","--noconfigfile","--vo","null","--ao","null","--machine","tbblue",
    "--tbblue-fast-boot-mode","--enable-remoteprotocol","--remoteprotocol-port",str(PORT),"--exit-after","120"],
    cwd=sys.argv[1] if len(sys.argv)>1 else ".",stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
for i in range(40):
    try: s=socket.create_connection(("127.0.0.1",PORT),timeout=10); break
    except OSError: time.sleep(0.25)
def rd():
    buf=b""
    while not buf.rstrip().endswith(b">"):
        c=s.recv(4096)
        if not c: break
        buf+=c
    return buf.decode(errors="replace")
def cmd(c):
    s.sendall((c+"\n").encode())
    t=rd()
    return t.replace("command@cpu-step> ","").replace("command> ","").strip()

rd()
time.sleep(2)
cmd("enter-cpu-step")

def out(port,val): cmd("write-port %d %d" % (port,val))
def mmu(slot): return int(cmd("tbblue-get-register %d" % (80+slot)).rstrip("H"),16)
def nr_write(reg,val): out(0x243B,reg); out(0x253B,val)
def nr_read(reg):
    # LD BC,243Bh / LD A,reg / OUT (C),A / LD B,25h / IN A,(C) / LD (9000h),A
    code=[0x01,0x3B,0x24,0x3E,reg,0xED,0x79,0x06,0x25,0xED,0x78,0x32,0x00,0x90]
    cmd("write-memory 32768 "+" ".join(str(b) for b in code))
    cmd("set-register PC=8000H")
    for _ in range(6): cmd("cpu-step")
    return int(cmd("read-memory 36864 1"),16)

# a +3 special módhoz +3-as gép kell (NR 0x03 = 3)
gep=int(cmd("tbblue-get-register 3").rstrip("H"),16) & 7
print("Géptípus (NR 0x03 bit 2-0): %d" % gep)

hibak = 0
def ell(nev, kapott, vart):
    global hibak
    ok = kapott == vart
    if not ok: hibak += 1
    print("  %-4s %-58s kapott %02XH, vart %02XH" % ("OK" if ok else "HIBA", nev, kapott, vart))

# alaphelyzet
nr_write(0x08, nr_read(0x08) | 0x80)   # zar feloldasa, ha volt
out(0x1FFD, 0x04); out(0xDFFD, 0x00); out(0x7FFD, 0x00)

print("1. Lapozaszar (7FFD bit 5) es NR 0x08 bit 7")
out(0x7FFD, 0x23)
ell("7FFD=23H: 3-as bank, zar", mmu(6), 0x06)
ell("NR 0x08 bit 7 = 0 (zarva)", nr_read(0x08) & 0x80, 0x00)
out(0x7FFD, 0x04)
ell("zarva: 7FFD=04H hatastalan", mmu(6), 0x06)
out(0xDFFD, 0x01)
ell("zarva: DFFD=01H hatastalan", mmu(6), 0x06)
nr_write(0x08, nr_read(0x08) | 0x80)
ell("NR 0x08 bit 7 irasa feloldja (olvasva 1)", nr_read(0x08) & 0x80, 0x80)
out(0x7FFD, 0x04)
ell("feloldva: 7FFD=04H -> 4-es bank", mmu(6), 0x08)

print("2. DFFD")
out(0xDFFD, 0x01)
ell("7FFD=04H, DFFD=01H -> 12-es bank", mmu(6), 0x18)
ell("MMU7", mmu(7), 0x19)
out(0xDFFD, 0x02)
ell("DFFD=02H -> 20-as bank", mmu(6), 0x28)
out(0x7FFD, 0x01)
ell("7FFD=01H (DFFD marad) -> 17-es bank", mmu(6), 0x22)
out(0xDFFD, 0x00); out(0x7FFD, 0x00)
ell("DFFD=00H, 7FFD=00H -> 0-s bank", mmu(6), 0x00)

print("3. NR 0x8E irasa")
out(0x7FFD, 0x08)
ell("7FFD=08H: arnyekkepernyo (NR 0x69 bit 6)", nr_read(0x69) & 0x40, 0x40)
nr_write(0x8E, 0x18)
ell("NR 0x8E=18H -> 1-es bank", mmu(6), 0x02)
ell("az arnyekkepernyo megmarad", nr_read(0x69) & 0x40, 0x40)
nr_write(0x8E, 0x01)
ell("NR 0x8E=01H (bit 3=0): MMU6 marad", mmu(6), 0x02)
nr_write(0x8E, 0x98)
ell("NR 0x8E=98H -> 9-es bank (bit 7 = DFFD bit 0)", mmu(6), 0x12)
out(0x7FFD, 0x20)
nr_write(0x8E, 0x28)
ell("zarva is mukodik: NR 0x8E=28H -> 2-es bank", mmu(6), 0x04)
nr_write(0x08, nr_read(0x08) | 0x80)

print("4. NR 0x8E olvasasa")
out(0xDFFD, 0x00); out(0x1FFD, 0x00); out(0x7FFD, 0x13)
ell("7FFD=13H (3-as bank, ROM bit) -> 39H", nr_read(0x8E), 0x39)
out(0xDFFD, 0x01)
ell("+ DFFD=01H -> B9H", nr_read(0x8E), 0xB9)
out(0xDFFD, 0x00); out(0x1FFD, 0x04)
ell("1FFD=04H (ROM felso bit) -> 3BH", nr_read(0x8E), 0x3B)

print("5. +3 special (csupa RAM) mod")
out(0x1FFD, 0x00); out(0x7FFD, 0x03)
out(0x1FFD, 0x01)
ell("1FFD=01H: MMU0 = 0-s lap", mmu(0), 0x00)
ell("1FFD=01H: MMU6 = 6-os lap (3-as bank)", mmu(6), 0x06)
out(0x1FFD, 0x07)
ell("1FFD=07H: MMU2 = 4,7,6,3 -> 14-es lap", mmu(2), 0x0E)
out(0x7FFD, 0x05)
out(0x1FFD, 0x00)
ell("kilepes: MMU0 = ROM (FFH)", mmu(0), 0xFF)
ell("kilepes: MMU2 = 5-os bank", mmu(2), 0x0A)
ell("kilepes: MMU4 = 2-es bank", mmu(4), 0x04)
ell("kilepes: MMU6 = a 7FFD bankja (5)", mmu(6), 0x0A)
nr_write(0x8E, 0x0C)
ell("NR 0x8E=0CH: special mod, MMU0 = 0-s lap", mmu(0), 0x00)
nr_write(0x8E, 0x08)
ell("NR 0x8E=08H: kilepes, MMU6 = 0-s bank", mmu(6), 0x00)
ell("NR 0x8E=08H: kilepes, MMU2 = 5-os bank", mmu(2), 0x0A)

out(0x1FFD, 0x04); out(0x7FFD, 0x00)
try: cmd("exit-emulator")
except Exception: pass
p.wait(timeout=10)
print("\n%s" % ("Minden rendben" if hibak == 0 else "%d hiba" % hibak))
sys.exit(1 if hibak else 0)
