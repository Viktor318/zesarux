#!/usr/bin/env python3
"""make_zesarux_keyboards.py <eightyone svg folder> [names...]

Fork (Viktor318/zesarux): the virtual keyboard pictures drawn again for the
EightyOne fork (Viktor318/EightyOne, Source/KBPics/svg), for ZEsarUX:

  src/keyboards/keyboard_<x>.png
                           the picture at 300% (1620 pixels wide), 256
                           colours, drawn at that size (the edges of the keys
                           on whole pixels); the .bmp of the same name is
                           deleted
  out/<x>.c                the key places for menu_items.c: the keys of the
                           table the picture had until now (and of its table of
                           additional keys), each moved onto the key of the new
                           picture in the same row and place. In units of the
                           original pictures (540 wide: half the pixels)
  out/<x>_keys.png         the picture with the keys of the tables marked

Needs what the EightyOne drawings need (Python 3 with numpy, scipy,
scikit-image, Pillow and fontTools, TeX Gyre Heros, rsvg-convert).
"""
import json
import os
import re
import subprocess
import sys
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
KEYBOARDS = os.path.join(HERE, '..', '..', 'src', 'keyboards')
MENU_ITEMS = os.path.join(HERE, '..', '..', 'src', 'menu', 'menu_items.c')
OUT = os.path.join(HERE, 'out')
WIDTH = 1620       # 300%: tres pixeles por pixel de los originales (desde 2026-11-19; antes 1080)
ORIGINAL = 540      # the coordinates of the tables are in pixels of the 540 wide originals

# picture: (EightyOne drawing, its places if not its own, table, table of additional keys, extra picture under it)
PICTURES = {
    '48': ('spectrum48', None, '48', None, None),
    '48p': ('spectrum128', None, 'plus', 'plus', None),
    '128': ('spectrum128', None, None, None, None),
    'p2': ('spectrumplus2', None, 'p2', 'p2', None),
    'p3': ('spectrumplus3', 'spectrumplus2', None, None, None),
    'zx81': ('zx81', None, 'zx81', None, None),
    'ts1000': ('ts1000', None, None, None, None),
    'zx80': ('zx80', None, 'zx80', None, 'zx80functions'),
    'ts1500': ('ts1500', None, 'timex_sinclair_1500', None, None),
    'tk85': ('tk85', None, 'tk85', None, None),
    'ace': ('ace', None, 'ace', None, None),
    'tc2048': ('tc2048', None, 'timex_computer', 'timex_computer', None),
    'ts2068': ('ts2068', None, None, None, None),
}
# The tables the new tables are made from (before they existed)
FROM = {'plus': ('48p', '48p', 'keyboard_48p.bmp'), 'zx81': ('zx8081', None, 'keyboard_zx81.bmp'),
        'zx80': ('zx8081', None, 'keyboard_zx80.bmp')}


def source():
    return open(MENU_ITEMS, encoding='latin-1').read()


def table(src, name):
    m = re.search(r'int keyboard_map_table_coords_%s\[[^\]]*\]=\{(.*?)\};' % name, src, re.S)
    if not m:
        return None
    nums = [int(x) for x in re.findall(r'-?\d+', re.sub(r'//[^\n]*', '', m.group(1)))]
    return [nums[i:i + 4] for i in range(0, len(nums), 4)]


def additional(src, name):
    m = re.search(r'keyboard_help_double_key keyboard_map_additional_%s\[\]=\{(.*?)\n\};' % name, src, re.S)
    if not m:
        return None
    out = []
    for e in re.finditer(r'\{([^{}]*)\}[^\n]*?(//[^\n]*)?\n', m.group(1) + '\n'):
        parts = [p.strip() for p in e.group(1).split(',')]
        if parts[:3] == ['0', '0', '0']:
            continue
        out.append({'rect': [int(p) for p in parts[:4]], 'rest': ', '.join(parts[4:]),
                    'comment': (e.group(2) or '').strip()})
    return out


def draw(svgdir, drawing, places, prefix):
    args = [sys.executable, os.path.join(HERE, 'eightyone_kb.py'), svgdir, drawing, str(WIDTH), prefix]
    if places:
        args.append(places)
    subprocess.check_call(args, stderr=subprocess.DEVNULL)
    return Image.open(prefix + '.png').convert('RGB'), json.load(open(prefix + '.json'))['keys']


def split_tall(keys):
    """A key two rows high (the ENTER of the Spectrum+, 128, +2, +3: an L) as
    the old tables have it: its column at the right of the upper row, and its
    lower row"""
    h = sorted(k[3] for k in keys)[len(keys) // 2]
    out = []
    for k in keys:
        if k[3] > 1.5 * h:
            x, y, w, hh = k
            upper = sorted([o for o in keys if o is not k and abs(o[1] - y) < h * 0.5 and o[0] < x + w])
            right = max(o[0] + o[2] for o in upper)
            gap = min(b[0] - (a[0] + a[2]) for a, b in zip(upper, upper[1:]))
            out.append([right + gap, y, x + w - right - gap, hh])
            out.append([x, y + hh - h, w, h])
        else:
            out.append(k)
    return out


def rows_of(points, tol):
    rows = []
    for p in sorted(points, key=lambda p: p[2]):
        if rows and abs(rows[-1][-1][2] - p[2]) < tol:
            rows[-1].append(p)
        else:
            rows.append([p])
    return rows


def mapping(old, new):
    """For each old rectangle [x, y, w, h], the new key in the same row (by
    the middles, in the box of all the keys) and nearest place in it"""
    def norm(rects):
        x0 = min(r[0] for r in rects)
        y0 = min(r[1] for r in rects)
        x1 = max(r[0] + r[2] for r in rects)
        y1 = max(r[1] + r[3] for r in rects)
        return [((r[0] + r[2] / 2 - x0) / (x1 - x0), (r[1] + r[3] / 2 - y0) / (y1 - y0)) for r in rects]
    o, n = norm(old), norm(new)
    h = sorted(k[3] for k in new)[len(new) // 2] / (max(k[1] + k[3] for k in new) - min(k[1] for k in new))
    nrows = rows_of([(i, x, y) for i, (x, y) in enumerate(n)], h * 0.5)
    out = []
    for x, y in o:
        row = min(nrows, key=lambda r: abs(sum(p[2] for p in r) / len(r) - y))
        out.append(min(row, key=lambda p: abs(p[1] - x))[0])
    return out


def to_bmp(im, path):
    sys.path.insert(0, SVGDIR)
    from make import colours256
    p = colours256(im)
    pal = p.getpalette()[:768]
    p.putpalette(pal + [0] * (768 - len(pal)))
    p.save(path)


def to_png(im, path):
    """la imagen como PNG de paleta (256 colores, como to_bmp); el BMP del
    mismo nombre, si queda, se borra (el programa usa el PNG)"""
    sys.path.insert(0, SVGDIR)
    from make import colours256
    p = colours256(im)
    pal = p.getpalette()[:768]
    p.putpalette(pal + [0] * (768 - len(pal)))
    p.save(path, optimize=True)
    bmp = path[:-4] + '.bmp'
    if os.path.exists(bmp):
        os.remove(bmp)


def c_rect(k):
    x, y, w, h = k
    return '%d,%d,%d,%d' % (round(x * ORIGINAL / WIDTH), round(y * ORIGINAL / WIDTH),
                            round((x + w) * ORIGINAL / WIDTH), round((y + h) * ORIGINAL / WIDTH))


def main():
    global SVGDIR
    SVGDIR = sys.argv[1]
    names = sys.argv[2:] or list(PICTURES)
    os.makedirs(OUT, exist_ok=True)
    src = source()
    for name in names:
        drawing, places, tname, aname, under = PICTURES[name]
        prefix = os.path.join(OUT, name)
        im, keys = draw(SVGDIR, drawing, places, prefix)
        if under:
            im2, _ = draw(SVGDIR, under, None, prefix + '_under')
            canvas = Image.new('RGB', (im.width, im.height + im2.height), im.getpixel((0, 0)))
            canvas.paste(im, (0, 0))
            canvas.paste(im2, (min(k[0] for k in keys), im.height))
            im = canvas
        to_png(im, os.path.join(KEYBOARDS, 'keyboard_%s.png' % name))
        if not tname:
            continue
        # the tables until now, and the size of their picture (in units of the original)
        tfrom, afrom, bmp = FROM.get(tname, (tname, aname, 'keyboard_%s.bmp' % name))
        t = table(src, tfrom)
        a = additional(src, afrom) if afrom else []
        oldrects = [None if r[0] >= 9999 else [r[0], r[1], r[2] - r[0], r[3] - r[1]] for r in t]
        oldrects += [[e['rect'][0], e['rect'][1], e['rect'][2] - e['rect'][0], e['rect'][3] - e['rect'][1]] for e in a]
        nkeys = split_tall(keys)
        m = iter(mapping([r for r in oldrects if r], nkeys))
        res = [None if r is None else nkeys[next(m)] for r in oldrects]
        lines = ['//Fork: para keyboard_%s.bmp (redibujado, ver tools/keyboards en la raiz del repositorio)' % name,
                 'int keyboard_map_table_coords_%s[40*4]={' % tname]
        for i in range(0, len(t), 5):
            lines.append('    ' + ',    '.join('9999,9999,9999,9999' if k is None else c_rect(k) for k in res[i:i + 5]) + ',')
        lines.append('};')
        if aname:
            lines += ['', 'keyboard_help_double_key keyboard_map_additional_%s[]={' % aname]
            for k, e in zip(res[len(t):], a):
                lines.append('    { %s, %s }, %s' % (c_rect(k).replace(',', ', '), e['rest'], e['comment']))
            lines += ['', '    { 0,0,0,0,NULL,0,NULL,0 }', '};']
        open(os.path.join(OUT, '%s.c' % name), 'w').write('\n'.join(lines) + '\n')
        mark = im.copy()
        dr = ImageDraw.Draw(mark)
        for i, k in enumerate(res):
            if k:
                x, y, w, h = k
                dr.rectangle([x, y, x + w - 1, y + h - 1], outline=(255, 0, 255) if i < len(t) else (0, 255, 0), width=2)
        mark.save(os.path.join(OUT, '%s_keys.png' % name))
        unused = [k for k in nkeys if k not in res]
        print(name, 'keys', len(nkeys), 'not in the tables:', unused)


if __name__ == '__main__':
    main()
