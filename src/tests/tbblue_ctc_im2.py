#!/usr/bin/env python3
"""A Next CTC-jének és hardveres IM2 megszakításvezérlőjének tesztje ZRCP-n át (zxnext.vhd, ctc.vhd, im2*.vhd
   szerint, a JNEXT ctc.cpp/im2.cpp-je alapján). Hét kis Z80-as program fut a 8000h címen, a megszakításokat
   számolja (A000h-tól), majd beolvassa az NR 0xC0/C5/C8/C9/20 regisztereket és a CTC0 számlálóját.
   A programok forrása: tbblue_ctc_im2_gen.py (asm-et ír, sjasmplus-szal fordítható); a lefordított
   bájtok alább vannak, így a teszthez nem kell assembler.
   Indítás az src mappából: python3 tests/tbblue_ctc_im2.py [zesarux mappa, alap: .]"""
import socket, subprocess, sys, time

PROGRAMOK = {
    "s1": bytes.fromhex(
        "f33100bfed9107002100a0062036002310fb3e90ed47ed5e219d8022ff9021cd8022a09021a98022a69021b58022a890"
        "21c18022b690ed912204ed91c000013b183ea5ed793e64ed79fbcd5080c365800e40060010fe0d20f9c9c5013b24ed79"
        "04ed78c1c9f33ec0cd5a803210a03ec5cd5a803211a03ec8cd5a803212a03ec9cd5a803213a03e20cd5a803214a0013b"
        "18ed783215a03e55321fa018fee52a00a0232200a0e1fbed4de52a02a0232202a0e1fbed4de52a04a0232204a0e1fbed"
        "4de52a06a0232206a0e1fbed4de52a08a0232208a0e1fbed4d"),
    "s2": bytes.fromhex(
        "f33100bfed9107002100a0062036002310fb3e90ed47ed5e219d8022ff9021cd8022a09021a98022a69021b58022a890"
        "21c18022b690ed912200ed91c0a1013b183ea5ed793e64ed79fbcd5080c365800e40060010fe0d20f9c9c5013b24ed79"
        "04ed78c1c9f33ec0cd5a803210a03ec5cd5a803211a03ec8cd5a803212a03ec9cd5a803213a03e20cd5a803214a0013b"
        "18ed783215a03e55321fa018fee52a00a0232200a0e1fbed4de52a02a0232202a0e1fbed4de52a04a0232204a0e1fbed"
        "4de52a06a0232206a0e1fbed4de52a08a0232208a0e1fbed4d"),
    "s3": bytes.fromhex(
        "f33100bfed9107002100a0062036002310fb3e90ed47ed5e219d8022ff9021cc8022a09021a98022a69021b48022a890"
        "21c08022b690ed912200ed91c0a1013b183ea5ed793e64ed79fbcd5080c365800e40060010fe0d20f9c9c5013b24ed79"
        "04ed78c1c9f33ec0cd5a803210a03ec5cd5a803211a03ec8cd5a803212a03ec9cd5a803213a03e20cd5a803214a0013b"
        "18ed783215a03e55321fa018fee52a00a0232200a0e1fbed4de52a02a0232202a0e1fbc9e52a04a0232204a0e1fbed4d"
        "e52a06a0232206a0e1fbed4de52a08a0232208a0e1fbed4d"),
    "s4": bytes.fromhex(
        "f33100bfed9107002100a0062036002310fb3e90ed47ed5e21a88022ff9021d88022a09021b48022a69021c08022a890"
        "21cc8022b690ed912204ed91c0a1013b183e05ed793e32ed79013b193ec5ed793e05ed79fbcd5b80c370800e40060010"
        "fe0d20f9c9c5013b24ed7904ed78c1c9f33ec0cd65803210a03ec5cd65803211a03ec8cd65803212a03ec9cd65803213"
        "a03e20cd65803214a0013b18ed783215a03e55321fa018fee52a00a0232200a0e1fbed4de52a02a0232202a0e1fbed4d"
        "e52a04a0232204a0e1fbed4de52a06a0232206a0e1fbed4de52a08a0232208a0e1fbed4d"),
    "s5": bytes.fromhex(
        "f33100bfed9107002100a0062036002310fb3e90ed47ed5e21968022ff9021c68022a09021a28022a69021ae8022a890"
        "21ba8022b690ed912364ed912202ed91c0a1fbcd4980c35e800e40060010fe0d20f9c9c5013b24ed7904ed78c1c9f33e"
        "c0cd53803210a03ec5cd53803211a03ec8cd53803212a03ec9cd53803213a03e20cd53803214a0013b18ed783215a03e"
        "55321fa018fee52a00a0232200a0e1fbed4de52a02a0232202a0e1fbed4de52a04a0232204a0e1fbed4de52a06a02322"
        "06a0e1fbed4de52a08a0232208a0e1fbed4d"),
    "s6": bytes.fromhex(
        "f33100bfed9107002100a0062036002310fb3e90ed47ed5e219a8022ff9021ca8022a09021a68022a69021b28022a890"
        "21be8022b690ed912204ed91c0a1ed91c9ffed912001fbcd4d80c362800e40060010fe0d20f9c9c5013b24ed7904ed78"
        "c1c9f33ec0cd57803210a03ec5cd57803211a03ec8cd57803212a03ec9cd57803213a03e20cd57803214a0013b18ed78"
        "3215a03e55321fa018fee52a00a0232200a0e1fbed4de52a02a0232202a0e1fbed4de52a04a0232204a0e1fbed4de52a"
        "06a0232206a0e1fbed4de52a08a0232208a0e1fbed4d"),
    "s7": bytes.fromhex(
        "f33100bfed9107002100a0062036002310fb3e90ed47ed5e21968022ff9021c68022a09021a28022a69021ae8022a890"
        "21ba8022b690ed91c8ffed91c400ed91c0a1fbcd4980c35e800e40060010fe0d20f9c9c5013b24ed7904ed78c1c9f33e"
        "c0cd53803210a03ec5cd53803211a03ec8cd53803212a03ec9cd53803213a03e20cd53803214a0013b18ed783215a03e"
        "55321fa018fee52a00a0232200a0e1fbed4de52a02a0232202a0e1fbed4de52a04a0232204a0e1fbed4de52a06a02322"
        "06a0e1fbed4de52a08a0232208a0e1fbed4d"),
}

BIN = sys.argv[1] if len(sys.argv) > 1 else "."
PORT = 10009

def futtat(nev, zsf=False):
    p = subprocess.Popen(["./zesarux", "--noconfigfile", "--vo", "null", "--ao", "null", "--machine", "tbblue",
                          "--tbblue-fast-boot-mode", "--enable-remoteprotocol", "--remoteprotocol-port", str(PORT),
                          "--exit-after", "60"] + (["--emulatorspeed", "10"] if zsf else []), cwd=BIN, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for i in range(80):
        try:
            s = socket.create_connection(("127.0.0.1", PORT), timeout=10); break
        except OSError:
            time.sleep(0.25)
    def rd():
        b = b""
        while not b.rstrip().endswith(b">"):
            x = s.recv(65536)
            if not x: break
            b += x
        return b.decode(errors="replace")
    def cmd(c):
        s.sendall((c + "\n").encode())
        return rd().replace("command@cpu-step> ", "").replace("command> ", "").strip()
    rd()
    time.sleep(2)
    cmd("enter-cpu-step")
    data = PROGRAMOK[nev]
    for i in range(0, len(data), 64):
        cmd("write-memory %d %s" % (0x8000 + i, " ".join(str(x) for x in data[i:i + 64])))
    cmd("write-memory 40991 0")
    cmd("set-register PC=8000H")
    cmd("exit-cpu-step")
    if zsf:
        # Futas kozben ZSF-mentes, a gep alaphelyzetbe allitasa (a CTC is), majd visszatoltes:
        # a programnak ugy kell befejezodnie, mintha nem szakitottuk volna meg
        # 10%-os sebesseggel a program kb. 0,6 mp-ig fut: a felenel mentunk
        time.sleep(0.3)
        cmd("enter-cpu-step")
        felig = cmd("read-memory 40991 1").strip().upper() != "55"
        cmd("snapshot-save /tmp/tbblue_ctc_im2_test.zsf")
        cmd("hard-reset-cpu")
        cmd("snapshot-load /tmp/tbblue_ctc_im2_test.zsf")
        cmd("exit-cpu-step")
    t0 = time.time()
    while time.time() - t0 < 15:
        if cmd("read-memory 40991 1").strip().upper() == "55": break
        time.sleep(0.2)
    cmd("enter-cpu-step")
    m = bytes.fromhex(cmd("read-memory 40960 32"))
    try: cmd("exit-emulator")
    except Exception: pass
    p.wait(timeout=10)
    w = lambda o: m[o] | (m[o + 1] << 8)
    return dict(kesz=m[0x1F] == 0x55, felig=(felig if zsf else None), pulse=w(0), ctc0=w(2), ctc1=w(4), ula=w(6), line=w(8),
                c0=m[0x10], c5=m[0x11], c8=m[0x12], c9=m[0x13], r20=m[0x14], ctc0_szamlalo=m[0x15])

hibak = 0
def ell(nev, ok, info):
    global hibak
    if not ok: hibak += 1
    print("  %-4s %-62s %s" % ("OK" if ok else "HIBA", nev, info))

r = futtat("s1"); print("1. Pulzus mod, CTC0 idozito (256-os eloszto, TC=100), ULA tiltva:", r)
ell("befejezodott", r["kesz"], "")
ell("a CTC0 a szokasos /INT pulzust adja (kb. 67 megszakitas)", 55 <= r["pulse"] <= 85, "pulse=%d" % r["pulse"])

r = futtat("s2"); print("2. HW IM2, CTC0 + ULA, sajat vektorok:", r)
ell("CTC0 a sajat vektoran (A6h), kb. 67", 55 <= r["ctc0"] <= 85, "ctc0=%d" % r["ctc0"])
ell("ULA a sajat vektoran (B6h), 2-4 keret", 2 <= r["ula"] <= 4, "ula=%d" % r["ula"])
ell("nincs pulzus-vektor (FFh)", r["pulse"] == 0, "pulse=%d" % r["pulse"])
ell("NR 0xC0 olvasva: A1h + IM 2 a 2-1. biten = A5h", r["c0"] == 0xA5, "c0=%02X" % r["c0"])
ell("NR 0xC5 olvasva: CTC0 engedelyezve = 01h", r["c5"] == 0x01, "c5=%02X" % r["c5"])

r = futtat("s3"); print("3. HW IM2, a CTC0 kezeloje RET-tel ter vissza (RETI nelkul):", r)
ell("CTC0 egyszer, utana szolgalatban marad", r["ctc0"] == 1, "ctc0=%d" % r["ctc0"])
ell("az alacsonyabb prioritasu ULA blokkolva", r["ula"] == 0, "ula=%d" % r["ula"])

r = futtat("s4"); print("4. HW IM2, CTC0 idozito -> CTC1 szamlalo lancolas:", r)
# 214144 T kesleltetes, a kezelo 96 T (elfogadas 19 + COUNT 59 + EI/RETI 18), CTC1 periodus 500 T: 214144/(500-96) = 530
# (2026-11-23 elott 506: a CTC nem kapta meg a megszakitas elfogadasanak idejet)
ell("CTC1 (szamlalo, TC=5) kb. 530 megszakitas", 520 <= r["ctc1"] <= 540, "ctc1=%d" % r["ctc1"])
ell("CTC0 nem kert megszakitast", r["ctc0"] == 0, "ctc0=%d" % r["ctc0"])

r = futtat("s5"); print("5. HW IM2, vonalmegszakitas (100. sor) + ULA:", r)
ell("vonalmegszakitas a sajat vektoran (A0h), 2-4", 2 <= r["line"] <= 4, "line=%d" % r["line"])
ell("ULA 2-4", 2 <= r["ula"] <= 4, "ula=%d" % r["ula"])

r = futtat("s6"); print("6. HW IM2, NR 0x20 = 01h (feltetel nelkuli CTC0):", r)
ell("CTC0 egyszer, engedelyezes nelkul is", r["ctc0"] == 1, "ctc0=%d" % r["ctc0"])
ell("NR 0xC9 allapot: CTC0 bit beallt", r["c9"] & 1 == 1, "c9=%02X" % r["c9"])
ell("NR 0x20 olvasva: CTC0 bit", r["r20"] & 1 == 1, "r20=%02X" % r["r20"])

r = futtat("s7"); print("7. HW IM2, ULA tiltva (NR 0xC4 = 0):", r)
ell("nincs ULA-megszakitas", r["ula"] == 0, "ula=%d" % r["ula"])
ell("az ULA allapotbitje (NR 0xC8 0. bit) beall", r["c8"] & 1 == 1, "c8=%02X" % r["c8"])

r = futtat("s4", zsf=True); print("8. Mint a 4., kozben ZSF-mentes, reset es visszatoltes:", r)
ell("a mentes a program futasa kozben tortent", r["felig"], "")
ell("befejezodott", r["kesz"], "")
ell("CTC1 a visszatoltes utan is szamol (kb. 530)", 500 <= r["ctc1"] <= 560, "ctc1=%d" % r["ctc1"])

print("\n%s" % ("Minden rendben" if hibak == 0 else "%d hiba" % hibak))
sys.exit(1 if hibak else 0)
