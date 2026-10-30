"""Draws the depth mesh (blue) over the Nest's sprite, and where the soldiers stand (their height,
16 units, at the DLL's places; the captain yellow, the others orange), at 5x: work/check_mesh.png.

    python check_mesh.py [name, default nest_6] [place_radius in map units, default 100]
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "roman-units", "tools"))
import bh3_render as b  # noqa: E402
from make_nest import A, C, E  # noqa: E402

WORK = os.path.join(HERE, "..", "work")
COORD_PER_UNIT = 192 * 2 / 84  # a 2x2 footprint: 384 map units, 84 world units
Z = 5

name = sys.argv[1] if len(sys.argv) > 1 else "nest_6"
place = float(sys.argv[2]) if len(sys.argv) > 2 else 100
im = Image.open(os.path.join(WORK, f"preview_{name}.png")).convert("RGBA")
a = np.array(im)[..., 3]
rgb = np.array(im)[..., :3].copy()
rgb[a == 145] = (40, 40, 40)
rgb[a == 0] = (90, 110, 60)
rgb[a == 248] = (200, 30, 30)
base = Image.fromarray(rgb).resize((im.width * Z, im.height * Z), Image.NEAREST)
m = b.load(os.path.join(WORK, "art", f"{name}z.bh3"))
v = m["v"]
x0, y0 = map(int, open(os.path.join(WORK, f"offsets_{name}.txt")).read().split())
sx = (x0 + A * (v[:, 0] - v[:, 1])) * Z
sy = (y0 + C * (v[:, 0] + v[:, 1]) - E * v[:, 2]) * Z
d = ImageDraw.Draw(base)
for t in m["f"][::4]:
    pts = [(sx[i], sy[i]) for i in t]
    d.line(pts + [pts[0]], fill=(0, 200, 255), width=1)
# the DLL's places (entrench.c, k_spot): an equilateral triangle around the centre, map x / y;
# the captain (yellow) towards the back corner, the other two (orange) towards the front
r = place / COORD_PER_UNIT
for k, (dx, dy) in enumerate(((-0.707, -0.707), (0.966, -0.259), (-0.259, 0.966))):
    x, y = dx * r, dy * r
    px, py = (x0 + A * (x - y)) * Z, (y0 + C * (x + y)) * Z
    d.line([(px, py), (px, py - 16 * E * Z)], fill=(255, 255, 0) if k == 0 else (255, 140, 0), width=3)
base.save(os.path.join(WORK, f"check_mesh_{name}.png"))
print(os.path.join(WORK, f"check_mesh_{name}.png"))
