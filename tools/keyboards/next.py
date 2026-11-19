#!/usr/bin/env python3
"""next.py <eightyone svg folder>

Fork (Viktor318/zesarux): el teclado del ZX Spectrum Next (TBBlue) dibujado de
nuevo, como cpc464.py: la imagen anterior es una foto, asi que las posiciones de
las teclas se han medido en ella (1080 x 402 pixeles; las tablas en unidades de
540) y se escriben aqui. Los textos son contornos de TeX Gyre Heros (GUST Font
License), de kbdraw.py del fork de EightyOne.

  src/keyboards/keyboard_next.png   la imagen, 1620 x 603 (300%; el dibujo es de
                                    1080 x 402), 256 colores
  out/next.svg, out/next.png        el dibujo
  out/next.c                        las posiciones para menu_items.c (unidades
                                    de 540), en el orden de las tablas antiguas
  out/next_keys.png                 la imagen con las teclas marcadas
"""
import os
import re
import subprocess
import sys
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_zesarux_keyboards as mz

ZOOM = 1.5               # la imagen guardada, respecto al dibujo
W, H = 1080, 402
F = 2                      # pixeles por unidad de las tablas (540 de ancho)

# colores
CASE = (10, 10, 11)        # el fondo, entre las teclas
BODY = (27, 28, 30)        # el cuerpo de la tecla
EDGE = (38, 39, 42)        # su borde superior, algo mas claro
TOP = (55, 57, 62)         # la cara de la tecla
INK = (240, 242, 244)      # las letras de la cara
INK2 = (205, 208, 212)     # las palabras pequenas sobre la cara

# filas: limites medidos en la foto
ROWS = [0, 78, 160, 241, 322, 402]
# limites de las teclas de cada fila, medidos en la foto
COLS = [
    [0, 76, 156, 236, 318, 399, 480, 562, 642, 724, 804, 883, 962, 1080],
    [0, 96, 196, 276, 359, 440, 521, 602, 683, 764, 845, 924, 1002],
    [0, 116, 216, 299, 380, 462, 542, 622, 702, 784, 865, 944],
    [0, 176, 279, 361, 442, 522, 602, 682, 764, 846, 925, 1080],
    [0, 78, 157, 238, 321, 402, 765, 845, 925, 1003, 1080],
]
GAP = 3                    # medio hueco entre teclas

# las teclas de cada fila: (tipo, textos)
#   'fn': palabra(s) en la cara;  'num': grafico, simbolo, cifra, dos palabras encima
#   'let': palabra clave, simbolo, letra, dos palabras encima;  'sym': un signo;  'arrow'
LEGENDS = [
    [('fn', 'BREAK'), ('fn', 'EDIT'),
     ('num', 1, '!', '1', 'BLUE', 'DEF FN'), ('num', 2, '@', '2', 'RED', 'FN'),
     ('num', 3, '#', '3', 'MAGENTA', 'LINE'), ('num', 4, '$', '4', 'GREEN', 'OPEN #'),
     ('num', 5, '%', '5', 'CYAN', 'CLOSE #'), ('num', 6, '&', '6', 'YELLOW', 'MOVE'),
     ('num', 7, "'", '7', 'WHITE', 'ERASE'), ('num', 8, '(', '8', '', 'POINT'),
     ('num', None, ')', '9', '', 'CAT'), ('num', None, '-', 'Ø', '#BLACK', 'FORMAT'),
     ('fn', 'DELETE')],
    [('fn', 'TRUE\nVIDEO'), ('fn', 'INV\nVIDEO'),
     ('let', 'PLOT', '<=', 'Q', 'SIN', 'ASN'), ('let', 'DRAW', '<>', 'W', 'COS', 'ACS'),
     ('let', 'REM', '>=', 'E', 'TAN', 'ATN'), ('let', 'RUN', '<', 'R', 'INT', 'VERIFY'),
     ('let', 'RAND', '>', 'T', 'RND', 'MERGE'), ('let', 'RETURN\nAND', '', 'Y', 'STR$', '['),
     ('let', 'IF\nOR', '', 'U', 'CHR$', ']'), ('let', 'INPUT\nAT', '', 'I', 'CODE', 'IN'),
     ('let', 'POKE', '', 'O', 'PEEK', 'OUT'), ('let', 'PRINT', '', 'P', 'TAB', '(C)')],
    [('fn', 'CAPS\nLOCK'), ('fn', 'GRAPH'),
     ('let', 'NEW\nSTOP', '', 'A', 'READ', '~'), ('let', 'SAVE\nNOT', '', 'S', 'RESTR', '|'),
     ('let', 'DIM\nSTEP', '', 'D', 'DATA', '\\'), ('let', 'FOR\nTO', '', 'F', 'SGN', '{'),
     ('let', 'GOTO\nTHEN', '', 'G', 'ABS', '}'), ('let', 'GOSUB', '↑', 'H', 'SQR', 'CIRCLE'),
     ('let', 'LOAD', '-', 'J', 'VAL', 'VAL$'), ('let', 'LIST', '+', 'K', 'LEN', 'SCRN$'),
     ('let', 'LET', '=', 'L', 'USR', 'ATTR')],
    [('fn', 'CAPS SHIFT'), ('fn', 'EXTEND\nMODE'),
     ('let', 'COPY', ':', 'Z', 'LN', 'BEEP'), ('let', 'CLEAR', '£', 'X', 'EXP', 'INK'),
     ('let', 'CONT', '?', 'C', 'LPRINT', 'PAPER'), ('let', 'CLS', '/', 'V', 'LLIST', 'FLASH'),
     ('let', 'BORDER', '*', 'B', 'BIN', 'BRIGHT'), ('let', 'NEXT', '', 'N', 'INKEY$', 'OVER'),
     ('let', 'PAUSE', '', 'M', 'PI', 'INVERSE'), ('arrow', '↑'), ('fn', 'CAPS SHIFT')],
    [('fn', 'SYMBOL\nSHIFT'), ('sym', ';'), ('sym', '"'), ('sym', ','), ('sym', '.'),
     ('fn', ''), ('arrow', '←'), ('arrow', '↓'), ('arrow', '→'), ('fn', 'SYMBOL\nSHIFT')],
]

# los signos que van a la izquierda bajo la palabra de encima (como en el teclado)
LEFT_SIGNS = ('[', ']', '{', '}')

# ENTER: una L, su parte de arriba en la fila 2 desde 1002, la de abajo en la fila 3 desde 944
ENTER = (1002, 944)

keys = []      # (x0, y0, x1, y1, leyenda)


def build():
    for r, (cols, legs) in enumerate(zip(COLS, LEGENDS)):
        y0, y1 = ROWS[r] + GAP, ROWS[r + 1] - GAP
        for i, leg in enumerate(legs):
            keys.append([cols[i] + GAP, y0, cols[i + 1] - GAP, y1, leg])


def hexcol(c):
    return '#%02x%02x%02x' % tuple(c)


def ev(v):
    return 2 * round(v / 2)


items = []


def rect(x0, y0, x1, y1, c, rx=0, snap=True):
    # bordes en pixeles pares: al 100% (la mitad) caen en pixeles enteros
    if snap:
        x0, y0, x1, y1 = ev(x0), ev(y0), ev(x1), ev(y1)
    items.append('<rect x="%g" y="%g" width="%g" height="%g" rx="%g" fill="%s"/>'
                 % (x0, y0, x1 - x0, y1 - y0, rx, hexcol(c)))


def text(s, cx, cy, h, fnt, c, ref='H', align='centre', maxw=None, snap=True):
    """s centrado en cx (align 'left': desde cx; 'right': hasta cx), sus
    mayusculas de h pixeles (la altura de ref), centradas en cy; mas estrecho
    si no cabe en maxw"""
    f = kbdraw.font(fnt)
    cap = f.outline(ref)[1]
    capH = cap[3] - cap[1]
    d, (x0, y0, x1, y1) = f.outline(s)
    sc = h / capH
    sx = sc
    if maxw and sc * (x1 - x0) > maxw:
        sx = maxw / (x1 - x0)
    w = sx * (x1 - x0)
    left = {'centre': cx - w / 2, 'left': cx, 'right': cx - w}[align]
    base = cy + h / 2
    if snap:
        base = ev(base)
        left = ev(left)
    items.append('<path fill="%s" transform="matrix(%.5f 0 0 %.5f %.3f %.3f)" d="%s"/>'
                 % (hexcol(c), sx, -sc, left - sx * x0, base, d))


def ink(s, cx, cy, h, fnt='herosb'):
    """un signo centrado por su tinta (para los signos sueltos)"""
    f = kbdraw.font(fnt)
    d, (x0, y0, x1, y1) = f.outline(s)
    sc = h / (y1 - y0)
    items.append('<path fill="%s" transform="matrix(%.5f 0 0 %.5f %.3f %.3f)" d="%s"/>'
                 % (hexcol(INK), sc, -sc, cx - sc * (x0 + x1) / 2, cy + sc * (y0 + y1) / 2, d))


STROKE = 3


def sign(ch, cx, cy, h=14):
    """los signos de la cara, de h pixeles (14: el tamano de las cifras
    pequenas): las barras de - = _ + como rectangulos"""
    k = h / 14
    bar, st = 13 * k, max(2.2, STROKE * k)
    if ch == '-':
        rect(cx - bar / 2, cy - st / 2, cx + bar / 2, cy + st / 2, INK, snap=False)
    elif ch == '=':
        for dy in (-3.5 * k, 3.5 * k):
            rect(cx - bar / 2, cy + dy - st / 2, cx + bar / 2, cy + dy + st / 2, INK, snap=False)
    elif ch == '+':
        rect(cx - bar / 2, cy - st / 2, cx + bar / 2, cy + st / 2, INK, snap=False)
        rect(cx - st / 2, cy - bar / 2, cx + st / 2, cy + bar / 2, INK, snap=False)
    elif len(ch) == 1 and ch in '*<>':
        ink(ch, cx, cy, {'*': 11, '<': 13, '>': 13}[ch] * k)
    elif ch == '↑':
        arrow(cx, cy, '↑', INK, L=15 * k, T=3 * k, head=8 * k, hw=5.5 * k)
    elif ch in ('<=', '>=', '<>'):
        # dos signos juntos, del tamano de los sueltos
        a, b = ch[0], ch[1]
        ink(a, cx - 8 * k, cy, 12 * k)
        if b == '=':
            for dy in (-3 * k, 3 * k):
                rect(cx + 2 * k, cy + dy - 1.2, cx + 13 * k, cy + dy + 1.2, INK, snap=False)
        else:
            ink(b, cx + 8 * k, cy, 12 * k)
    else:
        text(ch, cx, cy, h, 'herosb', INK, snap=False)


def arrow(cx, cy, d, c, L=30, T=6, head=15, hw=11, hollow=0):
    """una flecha de trazo T y largo L (con la punta), hacia d"""
    pts = [(-L / 2, -T / 2), (L / 2 - head, -T / 2), (L / 2 - head, -hw), (L / 2, 0),
           (L / 2 - head, hw), (L / 2 - head, T / 2), (-L / 2, T / 2)]
    rot = {'→': (1, 0), '←': (-1, 0), '↓': (0, 1), '↑': (0, -1)}[d]
    out = [(cx + x * rot[0] - y * rot[1], cy + x * rot[1] + y * rot[0]) for x, y in pts]
    if hollow:
        items.append('<polygon fill="none" stroke="%s" stroke-width="%g" stroke-linejoin="round" points="%s"/>'
                     % (hexcol(c), hollow, ' '.join('%g,%g' % p for p in out)))
    else:
        items.append('<polygon fill="%s" points="%s"/>' % (hexcol(c), ' '.join('%g,%g' % p for p in out)))


def block(n, cx, cy, s=16):
    """el grafico de bloques de la tecla n (1-8): un cuadrado con borde, con los
    cuartos del caracter 128+n del Spectrum llenos"""
    code = {1: 1, 2: 2, 3: 3, 4: 4, 5: 5, 6: 6, 7: 7, 8: 0}[n]
    x0, y0 = ev(cx - s / 2), ev(cy - s / 2)
    rect(x0, y0, x0 + s, y0 + s, INK, snap=False)
    rect(x0 + 2, y0 + 2, x0 + s - 2, y0 + s - 2, TOP, snap=False)
    h = (s - 4) / 2
    quads = {1: (1, 0), 2: (0, 0), 4: (1, 1), 8: (0, 1)}
    for bit, (qx, qy) in quads.items():
        if code & bit:
            rect(x0 + 2 + qx * h, y0 + 2 + qy * h, x0 + 2 + (qx + 1) * h, y0 + 2 + (qy + 1) * h, INK, snap=False)


def keycap(x0, y0, x1, y1, small):
    """el cuerpo y la cara de una tecla; small: deja sitio arriba para dos
    palabras. Devuelve la caja de la cara"""
    rect(x0, y0, x1, y1, BODY, 6)
    rect(x0, y0, x1, y0 + 3, EDGE, 2)
    fy0 = y0 + (22 if small else 10)
    face = (x0 + 5, fy0, x1 - 5, y1 - 4)
    # la cara: redondeada, mas abajo que arriba (como las teclas del Next)
    fx0, fy0, fx1, fy1 = face
    # arriba esquinas pequenas, abajo un arco ancho (casi medio circulo en las teclas pequenas)
    r1, r2 = 8, min((fx1 - fx0) / 2 - 1, 30, (fy1 - fy0) - 9)
    items.append('<path fill="%s" d="M%g %g H%g A%g %g 0 0 1 %g %g V%g A%g %g 0 0 1 %g %g H%g A%g %g 0 0 1 %g %g V%g A%g %g 0 0 1 %g %gZ"/>'
                 % (hexcol(TOP), fx0 + r1, fy0, fx1 - r1, r1, r1, fx1, fy0 + r1, fy1 - r2,
                    r2, r2, fx1 - r2, fy1, fx0 + r2, r2, r2, fx0, fy1 - r2, fy0 + r1,
                    r1, r1, fx0 + r1, fy0))
    return face


def draw():
    rect(0, 0, W, H, CASE)
    for x0, y0, x1, y1, leg in keys:
        kind = leg[0]
        small = kind in ('num', 'let')
        fx0, fy0, fx1, fy1 = keycap(x0, y0, x1, y1, small)
        cx = (fx0 + fx1) / 2
        if small:
            # las dos palabras de encima, en el cuerpo
            up1, up2 = leg[-2], leg[-1]
            kx = (x0 + x1) / 2
            ty = y0 + 3
            if up1.startswith('#'):
                # BLACK: letras oscuras en un recuadro claro
                t = up1[1:]
                rect(kx - 17, ty + 0.5, kx + 17, ty + 9, INK2, 1, snap=False)
                text(t, kx, ty + 4.75, 6, 'cnb', BODY, maxw=30, snap=False)
            elif up1:
                text(up1, kx, ty + 4.5, 7.5, 'cnb', INK2, maxw=x1 - x0 - 8, snap=False)
            if up2 in LEFT_SIGNS:
                # el signo a la izquierda, por debajo de la palabra
                ink(up2, kx - 22, ty + 12, 8, 'herosb')
            elif up2 == '~':
                # la tilde del teclado: una linea en zigzag
                pts = [(kx - 9, ty + 16), (kx - 4, ty + 10), (kx + 1, ty + 15), (kx + 5, ty + 10), (kx + 9, ty + 14)]
                items.append('<polyline fill="none" stroke="%s" stroke-width="1.8" points="%s"/>'
                             % (hexcol(INK2), ' '.join('%g,%g' % p for p in pts)))
            elif up2 in ('|', '\\'):
                ink(up2, kx - 6, ty + 13, 8, 'heros')
            elif up2:
                text(up2, kx, ty + 13.5, 7.5, 'cnb', INK2, maxw=x1 - x0 - 8, snap=False)
        if kind == 'num':
            _, g, sym, digit = leg[:4]
            if g is not None:
                block(g, cx - 13, fy0 + 15)
                sign(sym, cx + 14, fy0 + 15)
            else:
                sign(sym, cx + 14, fy0 + 15)
            text(digit, cx, fy1 - 13, 16, 'herosb', INK)
        elif kind == 'let':
            _, word, sym, letter = leg[:4]
            words = word.split('\n')
            for n, w in enumerate(words):
                text(w, cx, fy0 + 9 + n * 11, 8, 'herosb', INK, maxw=fx1 - fx0 - 8)
            if sym:
                sign(sym, fx1 - 15, fy0 + 22, 10)
            text(letter, cx, fy1 - 11, 15, 'herosb', INK)
        elif kind == 'fn':
            lines = leg[1].split('\n') if leg[1] else []
            cy = (fy0 + fy1) / 2
            for n, l in enumerate(lines):
                text(l, cx, cy + (n - (len(lines) - 1) / 2) * 13, 8, 'herosb', INK, maxw=fx1 - fx0 - 12)
        elif kind == 'sym':
            sign(leg[1], cx, (fy0 + fy1) / 2, 18)
        elif kind == 'arrow':
            arrow(cx, (fy0 + fy1) / 2, leg[1], INK, L=32, T=10, head=15, hw=13, hollow=2.6)
    # ENTER: una L sobre las filas 2 y 3
    xt, xb = ENTER
    y0, ym, y1 = ROWS[1] + GAP, ROWS[2], ROWS[3] - GAP
    x1 = W - GAP
    def lpath(xa, ya, xb2, yb, xl, ym2, c, rt, rb):
        # una L: arriba de xa a xb2, abajo de xl a xb2, con esquinas redondeadas
        d = (f'M{xa + rt} {ya} H{xb2 - rt} A{rt} {rt} 0 0 1 {xb2} {ya + rt} V{yb - rb} '
             f'A{rb} {rb} 0 0 1 {xb2 - rb} {yb} H{xl + rb} A{rb} {rb} 0 0 1 {xl} {yb - rb} '
             f'V{ym2 + rt} A{rt} {rt} 0 0 1 {xl + rt} {ym2} H{xa - rt} A{rt} {rt} 0 0 0 {xa} {ym2 - rt} '
             f'V{ya + rt} A{rt} {rt} 0 0 1 {xa + rt} {ya} Z')
        items.append('<path fill="%s" d="%s"/>' % (hexcol(c), d))
    lpath(xt + GAP, y0, x1, y1, xb + GAP, ym + GAP, BODY, 6, 6)
    lpath(xt + GAP + 5, y0 + 10, x1 - 5, y1 - 4, xb + GAP + 5, ym + GAP + 10, TOP, 8, 30)
    text('ENTER', (xb + x1) / 2 + 4, (ym + y1) / 2 + 4, 8, 'herosb', INK)
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">' % (W, H, W, H),
           '<!-- The keyboard of the ZX Spectrum Next, drawn again for the ZEsarUX fork '
           '(tools/keyboards/next.py). The letters are outlines of TeX Gyre Heros (GUST Font License). -->']
    return '\n'.join(out + items + ['</svg>'])


def places():
    """los rectangulos de las teclas (pixeles), ENTER como su parte de abajo
    (las dos filas en el ancho de la de arriba) y la de arriba aparte"""
    r = [k[:4] for k in keys]
    xt, xb = ENTER
    r.append([xt + GAP, ROWS[1] + GAP, W - GAP, ROWS[3] - GAP])
    r.append([xb + GAP, ROWS[2] + GAP, W - GAP, ROWS[3] - GAP])
    return r


def remap(rects, new):
    out = []
    for r in rects:
        if r[0] >= 9999:
            out.append(list(r))
            continue
        ocx, ocy = (r[0] + r[2]) / 2 * F, (r[1] + r[3]) / 2 * F

        def dist(k):
            dx = max(k[0] - ocx, 0, ocx - k[2])
            dy = max(k[1] - ocy, 0, ocy - k[3])
            return dx * dx + dy * dy + ((k[0] + k[2]) / 2 - ocx) ** 2 * 1e-3
        k = min(new, key=dist)
        out.append([round(k[0] / F), round(k[1] / F), round(k[2] / F) - 1, round(k[3] / F) - 1])
    return out


def main():
    global kbdraw
    sys.path.insert(0, sys.argv[1])
    import kbdraw
    mz.SVGDIR = sys.argv[1]
    build()
    os.makedirs(mz.OUT, exist_ok=True)
    svg = os.path.join(mz.OUT, 'next.svg')
    png = os.path.join(mz.OUT, 'next.png')
    open(svg, 'w').write(draw())
    # la imagen a 1,5 veces el dibujo (300%: tres pixeles por pixel del original);
    # los bordes en pixeles pares del dibujo caen asi en multiplos de 3
    subprocess.check_call(['rsvg-convert', '-z', str(ZOOM), svg, '-o', png])
    mz.SVGDIR = sys.argv[1]
    mz.to_png(Image.open(png).convert('RGB'), os.path.join(mz.KEYBOARDS, 'keyboard_next.png'))
    # las tablas y la imagen marcada, en las medidas del dibujo
    im = Image.open(png).convert('RGB').resize((W, H), Image.BOX)

    src = mz.source()
    old = mz.table(src, 'next')
    new = places()
    out = remap(old, new)
    # la tabla adicional: sus rectangulos, cada uno sobre su tecla nueva
    m = re.search(r'keyboard_help_double_key keyboard_map_additional_next\[\]=\{(.*?)\n\};', src, re.S)
    add_lines = []
    for line in m.group(1).split('\n'):
        a = re.match(r'(\s*\{\s*)(\d+),(\d+),(\d+),(\d+),(\s*.*)$', line)
        if not a or a.group(2) == '0' and a.group(3) == '0' and a.group(4) == '0':
            continue
        r = [int(a.group(i)) for i in (2, 3, 4, 5)]
        n = remap([r], new)[0]
        add_lines.append('%s%d,%d,%d,%d,%s' % (a.group(1), n[0], n[1], n[2], n[3], a.group(6)))
    rows = []
    for n in range(0, len(out), 5):
        rows.append('    ' + ', '.join('%d,%d,%d,%d' % tuple(k) for k in out[n:n + 5]) + ',')
    c = ['int keyboard_map_table_coords_next[40*4]={'] + rows + ['};', '',
         'keyboard_help_double_key keyboard_map_additional_next[]={'] + add_lines + \
        ['', '    { 0,0,0,0,NULL,0,NULL,0 }', '};']
    open(os.path.join(mz.OUT, 'next.c'), 'w').write('\n'.join(c) + '\n')

    mark = im.copy()
    d = ImageDraw.Draw(mark)
    for i, r in enumerate(out):
        d.rectangle([r[0] * F, r[1] * F, r[2] * F, r[3] * F], outline=(255, 0, 255), width=2)
        d.text((r[0] * F + 3, r[1] * F + 3), str(i), fill=(255, 255, 0))
    for line in add_lines:
        a = re.match(r'\s*\{\s*(\d+),(\d+),(\d+),(\d+),', line)
        r = [int(a.group(i)) for i in (1, 2, 3, 4)]
        d.rectangle([r[0] * F, r[1] * F, r[2] * F, r[3] * F], outline=(0, 255, 0), width=2)
    mark.save(os.path.join(mz.OUT, 'next_keys.png'))


if __name__ == '__main__':
    main()
