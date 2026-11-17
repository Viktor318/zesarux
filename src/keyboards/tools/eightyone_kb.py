#!/usr/bin/env python3
"""eightyone_kb.py <eightyone svg folder> <drawing> <width> <out prefix> [<places>]

Fork (Viktor318/zesarux): draws one of the keyboard pictures drawn again for
the EightyOne fork (Viktor318/EightyOne, Source/KBPics/svg) at the width the
ZEsarUX virtual keyboard shows at 200%, and writes

  <out prefix>.svg   the drawing
  <out prefix>.png   the picture (rsvg-convert)
  <out prefix>.json  the keys: [x, y, w, h] in pixels of the picture, in the
                     order of the drawing (as analyze.py found them)

The drawing scripts take the scale of the original pictures (505 x 223); the
edges of the keys fall on whole pixels at any scale. <places>: the analysis
the drawing uses, if not its own (spectrumplus3: spectrumplus2); without
one, the scale is that of the 505 pixel wide pictures (zx80functions).
"""
import json
import os
import runpy
import subprocess
import sys


def main():
    svgdir, name, width, prefix = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
    places = sys.argv[5] if len(sys.argv) > 5 else name
    if os.path.exists(os.path.join(svgdir, places + '.json')):
        scale = width / json.load(open(os.path.join(svgdir, places + '.json')))['size'][0]
    else:
        scale = width / 505
    sys.path.insert(0, svgdir)
    import kbdraw
    pictures = []
    original_svg = kbdraw.Picture.svg

    def svg(self, *args, **kwargs):
        pictures.append(self)
        return original_svg(self, *args, **kwargs)

    kbdraw.Picture.svg = svg
    sys.argv = [os.path.join(svgdir, name + '.py'), prefix + '.svg', repr(scale)]
    runpy.run_path(sys.argv[0], run_name='__main__')
    keys = []
    if pictures:
        p = pictures[-1]
        keys = [[round(v * p.S) for v in k] for k in p.newkeys]
    json.dump({'scale': scale, 'keys': keys}, open(prefix + '.json', 'w'))
    subprocess.check_call(['rsvg-convert', prefix + '.svg', '-o', prefix + '.png'])


if __name__ == '__main__':
    main()
