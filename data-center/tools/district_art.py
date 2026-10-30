"""Sprites for the three city districts (2x2 buildings) from their Gemini pictures
(gemini\\district_<kind>_v1.jpg, the user's, 2026-10-29): keyed off the white background, scaled
once to a 2x2 plot, the red roof edges thinned to a line of player colour, one grey grade with the
communist buildings, and a box depth mesh.

    python district_art.py

Writes work\\art\\district_<kind>.tga, district_<kind>z.bh3 and work\\offsets_district_<kind>.txt
(spritexoff spriteyoff). Projection of the plot as in roman-buildings\\tools\\depth_mesh.py (checked
on game screenshots, 58 world units a tile): sx = x0 + A (x - y), sy = y0 + C (x + y) - E z.
"""
import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
sys.path.insert(0, os.path.join(ROOT, "entrenchment", "tools"))
sys.path.insert(0, os.path.join(ROOT, "paradrop", "tools"))
sys.path.insert(0, os.path.join(ROOT, "roman-buildings", "tools"))
from make_nest import write_bh3, write_tga  # noqa: E402
from grade_sprite import grade  # noqa: E402

GEM = os.path.join(HERE, "..", "gemini")
WORK = os.path.join(HERE, "..", "work")
A, C, E = 0.70, 0.31, 0.90
TILE = 58.0
HALF = 0.92 * TILE           # the building's footprint: a little inside its 2x2 plot (half side)
SOLID, TEAM = 255, 248
KEEP = 4                     # rows of a red roof edge that stay player colour
KINDS = ("housing", "commerce", "industry")


def key(path):
    a = np.asarray(Image.open(path).convert("RGB")).astype(int)
    bg = (a.min(2) > 222) & ((a.max(2) - a.min(2)) < 16)
    lab, _ = ndimage.label(bg)
    border = np.unique(np.r_[lab[0], lab[-1], lab[:, 0], lab[:, -1]])
    body = ~np.isin(lab, border[border > 0])
    body = ndimage.binary_erosion(body, np.ones((3, 3)))
    lab, n = ndimage.label(body)
    body = lab == (np.argmax(ndimage.sum(body, lab, range(1, n + 1))) + 1)
    im = Image.fromarray(np.dstack([a, body * 255]).astype(np.uint8), "RGBA")
    return im.crop(im.getbbox())


def scale(im, s):
    a = np.asarray(im).astype(np.float64)
    al = a[..., 3:4] / 255
    pre = Image.fromarray(np.dstack([a[..., :3] * al, a[..., 3]]).astype(np.uint8), "RGBA")
    size = (max(1, round(im.width * s)), max(1, round(im.height * s)))
    r = np.asarray(pre.resize(size, Image.LANCZOS)).astype(np.float64)
    al = r[..., 3:4] / 255
    rgb = np.where(al > 0.02, r[..., :3] / np.maximum(al, 1e-6), 0)
    return np.dstack([np.clip(rgb, 0, 255), (al[..., 0] > 0.5) * 255]).astype(np.uint8)


def thin_line(a):
    """A red roof edge: its lowest KEEP rows become player colour, the rest concrete."""
    body = a[..., 3] == SOLID
    r, g, b = (a[..., i].astype(int) for i in range(3))
    red = body & (r > 1.6 * g) & (r > 1.6 * b) & (r > 60)
    red = ndimage.binary_closing(red, np.ones((3, 3))) & body
    lum = (r * 299 + g * 587 + b * 114) // 1000
    walls = body & ~red & ((a[..., :3].max(2).astype(int) - a[..., :3].min(2)) < 25)
    wall = a[..., :3][walls].mean(0)
    below = np.zeros_like(red)
    for d in range(1, KEEP + 1):
        sh = np.zeros_like(red)
        sh[:-d] = body[d:] & ~red[d:]
        below |= sh
    team = red & below
    concrete = red & ~team
    out = a.copy()
    grey = np.clip(lum * 95 // 70, 40, 150)
    out[team, 0] = out[team, 1] = out[team, 2] = grey[team]
    out[team, 3] = TEAM
    if concrete.any():
        k = 1.15 * wall.mean() / max(1.0, lum[concrete].mean())
        out[concrete, :3] = np.clip(lum[concrete][:, None] * k * wall / wall.mean(), 0, 255)
    return out, int(team.sum())


def box_mesh(h, z):
    """The block as the depth mesh: four walls and the top, both sides of every face."""
    c = [(-h, -h), (h, -h), (h, h), (-h, h)]
    verts = [(x, y, 0.0) for x, y in c] + [(x, y, z) for x, y in c]
    faces = [(4, 5, 6, 7)] + [(i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i) for i in range(4)]
    tris = []
    for a, b, cc, dd in faces:
        tris += [(a, b, cc), (a, cc, dd), (a, cc, b), (a, dd, cc)]
    return np.array(verts, np.float32), np.array(tris, np.uint16)


def main():
    os.makedirs(os.path.join(WORK, "art"), exist_ok=True)
    width = 2 * A * 2 * HALF                    # the footprint's screen width
    for kind in KINDS:
        im = key(os.path.join(GEM, f"district_{kind}_v1.jpg"))
        a = scale(im, width / im.width)
        a, team = thin_line(a)
        a, *_ = grade(a)
        h, w = a.shape[:2]
        m = 2
        canvas = np.zeros((h + 2 * m, w + 2 * m, 4), np.uint8)
        canvas[m:m + h, m:m + w] = a
        x0 = m + w / 2                           # the footprint's centre: under the middle,
        y0 = m + h - 1 - C * 2 * HALF            # half the diamond above the front corner
        z = max(10.0, (y0 - C * 2 * HALF - m) / E)
        name = f"district_{kind}"
        write_tga(Image.fromarray(canvas, "RGBA"), os.path.join(WORK, "art", f"{name}.tga"))
        write_bh3(*box_mesh(HALF, z), os.path.join(WORK, "art", f"{name}z.bh3"))
        open(os.path.join(WORK, f"offsets_{name}.txt"), "w").write(f"{round(x0)} {round(y0)}\n")
        print(f"{name}: {canvas.shape[1]}x{canvas.shape[0]}, origin ({round(x0)}, {round(y0)}), height {z:.0f}, "
              f"player colour {team}")


if __name__ == "__main__":
    main()
