#!/usr/bin/env python3
"""cpc464.py <eightyone svg folder>

Fork (Viktor318/zesarux): the keyboard of the Amstrad CPC 464 (and CPC 4128)
drawn again, as the pictures of make_zesarux_keyboards.py: the old picture is
a photo, so there is nothing to analyse; the places of the keys are measured
on it (1280 x 490 pixels, twice its 640 x 245 original) and written here.
The texts are outlines of TeX Gyre Heros (GUST Font License), from kbdraw.py
of the EightyOne fork.

  src/keyboards/keyboard_cpc_464.bmp   the picture, 1280 x 490, 256 colours
  out/cpc_464.svg, out/cpc_464.png     the drawing
  out/cpc_464.c                        the key places for menu_items.c (640
                                       units), in the order of the old table
  out/cpc_464_keys.png                 the picture with the keys marked
"""
import os
import subprocess
import sys
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_zesarux_keyboards as mz

W, H = 1280, 490
P = 64.6            # the pitch of the keys

# colours (from the photo)
CASE = (79, 89, 101)
PLATE = (70, 80, 93)
PLATE_EDGE = (58, 66, 77)
WELL = (28, 30, 32)
INK = (226, 230, 232)
INK_COLOUR = (244, 246, 248)     # las palabras de las teclas de color (rojas, verdes, azules)
PLATE_INK = (214, 220, 226)
KEYS = {
    #      skirt           top
    'k': ((34, 40, 46), (54, 64, 74)),
    'g': ((58, 132, 90), (99, 172, 128)),
    'r': ((186, 48, 44), (236, 82, 76)),
    'b': ((44, 96, 176), (68, 132, 220)),
}

keys = []   # (x0, y0, x1, y1, colour, label, kind)


def key(x0, y0, x1, y1, c, label, kind='sym'):
    keys.append([round(x0), round(y0), round(x1), round(y1), c, label, kind])


ROWS = [160, 224, 288, 352, 418]
KH = 62

# 1st row
y = ROWS[0]
key(11, y, 71, y + KH, 'r', 'ESC', 'fn')
for i, t in enumerate(['!1', '"2', '#3', '$4', '%5', '&6', "'7", '(8', ')9', '_Ø', '=-', '£↑']):
    x = 75 + i * P
    key(x, y, x + 60, y + KH, 'k', t, 'two')
key(75 + 12 * P, y, 75 + 12 * P + 60, y + KH, 'k', 'CLR', 'fn')
key(917, y, 1008, y + KH, 'g', 'DEL', 'fn')
# 2nd row
y = ROWS[1]
key(12, y, 103, y + KH, 'g', 'TAB', 'fn')
for i, t in enumerate('QWERTYUIOP'):
    x = 107 + i * P
    key(x, y, x + 60, y + KH, 'k', t, 'big')
key(107 + 10 * P, y, 107 + 10 * P + 60, y + KH, 'k', '|@', 'two')
key(107 + 11 * P, y, 107 + 11 * P + 60, y + KH, 'k', '{[', 'two')
ENTER = (882, ROWS[1], 1008, ROWS[2] + KH, 898)     # an L: its upper row from 882, the lower from 898
# 3rd row
y = ROWS[2]
key(12, y, 120, y + KH, 'g', 'CAPS\nLOCK', 'fn')
for i, t in enumerate('ASDFGHJKL'):
    x = 124 + i * P
    key(x, y, x + 60, y + KH, 'k', t, 'big')
for i, t in enumerate(['*:', '+;', '}]']):
    x = 124 + (9 + i) * P
    key(x, y, x + 60, y + KH, 'k', t, 'two')
# 4th row
y = ROWS[3]
key(12, y, 152, y + KH, 'g', 'SHIFT', 'fn')
for i, t in enumerate('ZXCVBNM'):
    x = 156 + i * P
    key(x, y, x + 60, y + KH, 'k', t, 'big')
for i, t in enumerate(['<,', '>.', '?/', '`\\']):
    x = 156 + (7 + i) * P
    key(x, y, x + 60, y + KH, 'k', t, 'two')
key(156 + 11 * P, y, 1008, y + KH, 'g', 'SHIFT', 'fn')
# 5th row
y = ROWS[4]
key(216, y, 786, y + KH, 'k', '', 'fn')
key(792, y, 856, y + KH, 'g', 'CTRL', 'fn')

# the cursor keys and COPY
CX = [1079, 1145, 1211]
CY = [6, 70, 134]
key(CX[1], CY[0], CX[1] + 62, CY[0] + 62, 'k', '↑', 'arrow')
key(CX[0], CY[1], CX[0] + 62, CY[1] + 62, 'k', '←', 'arrow')
key(CX[1], CY[1], CX[1] + 62, CY[1] + 62, 'g', 'COPY', 'fn')
key(CX[2], CY[1], CX[2] + 62, CY[1] + 62, 'k', '→', 'arrow')
key(CX[1], CY[2], CX[1] + 62, CY[2] + 62, 'k', '↓', 'arrow')
# the numeric keypad (f0 - f9, f.)
for r, row in enumerate([('7', '8', '9'), ('4', '5', '6'), ('1', '2', '3'), ('Ø', '.', None)]):
    for c, t in enumerate(row):
        if t:
            key(CX[c], ROWS[r + 1], CX[c] + 62, ROWS[r + 1] + KH, 'k', t, 'big')
key(CX[2], ROWS[4], CX[2] + 62, ROWS[4] + KH, 'b', 'ENTER', 'fn')


# ---- drawing ----------------------------------------------------------------
def hexcol(c):
    return '#%02x%02x%02x' % tuple(c)


items = []


def ev(v):
    return 2 * round(v / 2)


def rect(x0, y0, x1, y1, c, rx=0, snap=True):
    # bordes en pixeles pares: al 100% (la mitad) caen en pixeles enteros, nitidos
    if snap:
        x0, y0, x1, y1 = ev(x0), ev(y0), ev(x1), ev(y1)
    items.append('<rect x="%g" y="%g" width="%g" height="%g" rx="%g" fill="%s"/>'
                 % (x0, y0, x1 - x0, y1 - y0, rx, hexcol(c)))


def text(s, cx, cy, h, fnt, c, ref='H', align='centre', snap=False):
    """s centred on cx (align 'left': from cx), its capitals h pixels high
    (as high as ref), centred on cy"""
    f = kbdraw.font(fnt)
    cap = f.outline(ref)[1]
    capH = cap[3] - cap[1]
    d, (x0, y0, x1, y1) = f.outline(s)
    sc = h / capH
    left = cx - sc * (x1 - x0) / 2 if align == 'centre' else cx
    base = cy + h / 2
    if snap:
        # la linea base y el borde izquierdo de la tinta en pixeles pares:
        # al 100% (la mitad) los trazos horizontales caen en pixeles enteros
        base = ev(base)
        left = ev(left)
    items.append('<path fill="%s" transform="matrix(%.5f 0 0 %.5f %.3f %.3f)" d="%s"/>'
                 % (hexcol(c), sc, -sc, left - sc * x0, base, d))


def arrow(cx, cy, d, c, L=22, T=4, head=12, hw=7.5):
    """an arrow of a line T thick, L long (with the head), pointing d"""
    pts = [(-L / 2, -T / 2), (L / 2 - head, -T / 2), (L / 2 - head, -hw), (L / 2, 0),
           (L / 2 - head, hw), (L / 2 - head, T / 2), (-L / 2, T / 2)]
    rot = {'→': (1, 0), '←': (-1, 0), '↓': (0, 1), '↑': (0, -1)}[d]
    out = []
    for x, y in pts:
        out.append((cx + x * rot[0] - y * rot[1], cy + x * rot[1] + y * rot[0]))
    items.append('<polygon fill="%s" points="%s"/>' % (hexcol(c), ' '.join('%g,%g' % p for p in out)))


# Los signos pequeños (* + - = _ < >) a tamaño normal, como las cifras: las
# barras de - = _ + como rectángulos del grosor del trazo de las cifras
SIGN_H = 14         # la altura de las cifras
STROKE = 2.6        # el grosor de su trazo (Heros Bold)
BAR = 12            # el ancho de - = _ +


def sign(ch, cx, cy):
    h = SIGN_H
    if ch == '-':
        rect(cx - BAR / 2, cy - STROKE / 2, cx + BAR / 2, cy + STROKE / 2, INK, snap=False)
    elif ch == '=':
        for dy in (-3, 3):
            rect(cx - BAR / 2, cy + dy - STROKE / 2, cx + BAR / 2, cy + dy + STROKE / 2, INK, snap=False)
    elif ch == '_':
        rect(cx - BAR / 2 - 0.5, cy + h / 2 - STROKE, cx + BAR / 2 + 0.5, cy + h / 2, INK, snap=False)
    elif ch == '+':
        rect(cx - BAR / 2, cy - STROKE / 2, cx + BAR / 2, cy + STROKE / 2, INK, snap=False)
        rect(cx - STROKE / 2, cy - BAR / 2, cx + STROKE / 2, cy + BAR / 2, INK, snap=False)
    elif ch in '*<>':
        # el signo de la fuente, su tinta de la altura dada, centrada
        f = kbdraw.font('herosb')
        d, (x0, y0, x1, y1) = f.outline(ch)
        ih = {'*': 10, '<': 12, '>': 12}[ch]
        sc = ih / (y1 - y0)
        items.append('<path fill="%s" transform="matrix(%.5f 0 0 %.5f %.3f %.3f)" d="%s"/>'
                     % (hexcol(INK), sc, -sc, cx - sc * (x0 + x1) / 2, cy + sc * (y0 + y1) / 2, d))
    elif ch in ':;,.':
        # la puntuación en negrita: puntos más visibles
        text(ch, cx, cy, h, 'herosb', INK)
    elif ch == '`':
        text(ch, cx, cy, h * 1.4, 'herosb', INK)
    else:
        text(ch, cx, cy, h, 'herosb', INK)


FN_H = 11           # la altura de las palabras de las teclas de funcion
# las de las teclas de color (Viktor): mas altas, mas blancas y en pixeles pares
FN_H_COLOUR, FN_LINE_COLOUR, FN_MARGIN_COLOUR = 12, 16, 16


def fn_text(t, cx, cy, maxw, colour=False):
    """una palabra de una tecla de funcion: Heros Cn Bold, FN_H de alto, mas
    baja si no cabe en maxw; en una tecla de color (colour) FN_H_COLOUR de
    alto, en INK_COLOUR y en pixeles pares"""
    f = kbdraw.font('cnb')
    capb = f.outline('H')[1]
    d, (x0, y0, x1, y1) = f.outline(t)
    h = min(FN_H_COLOUR if colour else FN_H, maxw * (capb[3] - capb[1]) / (x1 - x0))
    text(t, cx, cy, h, 'cnb', INK_COLOUR if colour else INK, snap=colour)


def cap(x0, y0, x1, y1, c):
    skirt, top = KEYS[c]
    rect(x0, y0, x1, y1, skirt, 5)
    rect(x0 + 6, y0 + 4, x1 - 6, y1 - 12, top, 4)
    return (ev(x0) + ev(x1)) / 2, (ev(y0) + 4 + ev(y1) - 12) / 2


def draw():
    rect(0, 0, W, H, CASE)
    # the name plate
    rect(30, 40, 1018, 126, PLATE_EDGE, 3)
    rect(32, 42, 1016, 124, PLATE, 2)
    text('AMSTRAD', 44, 96, 20, 'cnb', PLATE_INK, align='left')
    text('64K COLOUR PERSONAL COMPUTER', 176, 103, 12, 'herosb', PLATE_INK, align='left')
    text('CPC464', 694, 96, 28, 'cnb', (236, 240, 244), align='left')
    # the colour badge
    rect(838, 84, 914, 108, (236, 240, 244), 2)
    for i, c in enumerate([(220, 40, 40), (40, 170, 70), (50, 110, 220)]):
        rect(843 + 24 * i, 89, 861 + 24 * i, 103, c, 2)
    text('COLOUR', 920, 102, 12, 'herosb', PLATE_INK, align='left')
    text('ON', 944, 71, 9, 'herosb', PLATE_INK, align='left')
    items.append('<circle cx="975" cy="67" r="6" fill="#e01818"/>')
    # the wells of the keys
    rect(7, 156, 1012, 484, WELL, 4)
    rect(1075, 220, 1277, 484, WELL, 4)
    for x0, y0, x1, y1 in [(CX[1] - 4, 2, CX[1] + 66, 200), (CX[0] - 4, CY[1] - 4, CX[2] + 66, CY[1] + 66)]:
        rect(x0, y0, x1, y1, WELL, 4)
    # ENTER, an L
    x0, y0, x1, y1, xl = ENTER
    sk, tp = KEYS['b']
    items.append('<path fill="%s" d="M%g %gH%gV%gH%gV%gH%gZ"/>'
                 % (hexcol(sk), x0, y0, x1, y1, xl, y0 + KH, x0))
    items.append('<path fill="%s" d="M%g %gH%gV%gH%gV%gH%gZ"/>'
                 % (hexcol(tp), x0 + 6, y0 + 4, x1 - 6, y1 - 11, xl + 6, y0 + KH - 11, x0 + 6))
    fn_text('ENTER', (xl + x1) / 2, (y0 + y1) / 2 - 4, 90, colour=True)
    for x0, y0, x1, y1, c, t, kind in keys:
        cx, cy = cap(x0, y0, x1, y1, c)
        if kind == 'big':
            text(t, cx, cy, 20, 'herosb', INK, ref='H')
        elif kind == 'two':
            up, lo = t[0], t[1]
            sign(up, cx, cy - 10)
            sign(lo, cx, cy + 11)
        elif kind == 'arrow':
            arrow(cx, cy, t, INK)
        elif t:
            lines = t.split('\n')
            colour = c != 'k'
            line, margin = (FN_LINE_COLOUR, FN_MARGIN_COLOUR) if colour else (15, 18)
            for n, l in enumerate(lines):
                fn_text(l, cx, cy + (n - (len(lines) - 1) / 2) * line, x1 - x0 - margin, colour)
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">' % (W, H, W, H),
           '<!-- The keyboard of the Amstrad CPC 464, drawn again for the ZEsarUX fork '
           '(tools/keyboards/cpc464.py). The letters are outlines of TeX Gyre Heros (GUST Font License). -->']
    return '\n'.join(out + items + ['</svg>'])


# ---- the table --------------------------------------------------------------
def places():
    """the rectangles of the keys (1280 pixels), ENTER as its lower part (the
    whole width of both rows) and its upper row apart"""
    r = [k[:4] for k in keys]
    x0, y0, x1, y1, xl = ENTER
    r.append([xl, y0, x1, y1])
    return r


def main():
    global kbdraw
    sys.path.insert(0, sys.argv[1])
    import kbdraw
    mz.SVGDIR = sys.argv[1]
    os.makedirs(mz.OUT, exist_ok=True)
    svg = os.path.join(mz.OUT, 'cpc_464.svg')
    png = os.path.join(mz.OUT, 'cpc_464.png')
    open(svg, 'w').write(draw())
    subprocess.check_call(['rsvg-convert', svg, '-o', png])
    im = Image.open(png).convert('RGB')
    mz.to_bmp(im, os.path.join(mz.KEYBOARDS, 'keyboard_cpc_464.bmp'))

    src = mz.source()
    old = mz.table(src, 'cpc_464')
    new = places()
    f = 1280 / 640
    lines = []
    out = []
    for i, r in enumerate(old):
        if r[0] >= 9999:
            out.append(list(r))
            continue
        ocx, ocy = (r[0] + r[2]) / 2 * f, (r[1] + r[3]) / 2 * f

        def dist(k):
            dx = max(k[0] - ocx, 0, ocx - k[2])
            dy = max(k[1] - ocy, 0, ocy - k[3])
            return dx * dx + dy * dy + ((k[0] + k[2]) / 2 - ocx) ** 2 * 1e-3
        k = min(new, key=dist)
        out.append([round(k[0] / f), round(k[1] / f), round(k[2] / f) - 1, round(k[3] / f) - 1])
    used = set(tuple(k) for k in out)
    rows = []
    for n in range(0, len(out), 8):
        rows.append(', '.join('%d,%d,%d,%d' % tuple(k) for k in out[n:n + 8]) + ',')
    x0, y0, x1, y1, xl = ENTER
    rs = [k for k in keys if k[5] == 'SHIFT' and k[0] > 500][0]
    add = [('%d,%d,%d,%d' % (round(rs[0] / f), round(rs[1] / f), round(rs[2] / f) - 1, round(rs[3] / f) - 1),
            '&cpc_keyboard_table[2], 32,   NULL,0', '//shift derecho'),
           ('%d,%d,%d,%d' % (round(x0 / f), round(y0 / f), round(x1 / f) - 1, round((y0 + KH) / f) - 1),
            '&cpc_keyboard_table[2], 4,    NULL,0', '//enter, fila superior')]
    c = ['int keyboard_map_table_coords_cpc_464[8*10*4]={'] + rows + ['};', '',
         'keyboard_help_double_key keyboard_map_additional_cpc_464[]={']
    c += ['    { %s,  %s }, %s' % a for a in add]
    c += ['', '    { 0,0,0,0,NULL,0,NULL,0 }', '};']
    open(os.path.join(mz.OUT, 'cpc_464.c'), 'w').write('\n'.join(c) + '\n')

    mark = im.copy()
    d = ImageDraw.Draw(mark)
    for i, r in enumerate(out):
        if r[0] >= 9999:
            continue
        d.rectangle([r[0] * f, r[1] * f, r[2] * f, r[3] * f], outline=(255, 0, 255), width=2)
        d.text((r[0] * f + 3, r[1] * f + 3), str(i), fill=(255, 255, 0))
    mark.save(os.path.join(mz.OUT, 'cpc_464_keys.png'))
    # keys of the picture no entry of the table falls on
    missing = [k[5] for k in keys if not any(abs(k[0] / f - o[0]) <= 1 and abs(k[1] / f - o[1]) <= 1 for o in out)]
    print('keys without an entry:', missing)


if __name__ == '__main__':
    main()
