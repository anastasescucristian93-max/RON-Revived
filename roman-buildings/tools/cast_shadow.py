"""Casts a building sprite's ground shadow from its 3D shape and the game's light, instead of
make_sprite's per-column guess (which treated every pixel as standing over the lowest pixel of
its column, so a church tower threw a flat grey slab, 2026-10-31).

    python cast_shadow.py fit <texture> [<texture> ...]       the light, fitted on the originals
    python cast_shadow.py cast <texture> <in.png> <out.png> [u v]

The shape is the depth mesh's height field (depth_mesh.heights, the camera projection fitted on 11
of the game's buildings): on a ground grid, how high the sprite is solid straight above each
point. Each column of height h casts the ground area its cell sweeps from (x, y) to
(x + u h, y + v h): a light ray falls u, v world units sideways per unit of height. Those areas
drawn with the camera projection at z = 0, minus the building's own pixels, are the shadow
(alpha 145).

fit: tries (u, v) on a grid and keeps the one whose cast shadow of the game's original sprite
best matches that sprite's own shadow (intersection over union), over all the textures given.
cast: uses the fitted light (LIGHT below) unless u v are given.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

from depth_mesh import GAME, PAD, build_entries, heights, mesh_vertices, project

SOLID, TEAM, SHADOW = 255, 248, 145
# Direction chosen by eye on the originals (library_0m, fort_0m, city_0ma, 2026-10-31): their
# shadows fall to the lower left, along -x. "fit" picked (0, 0.8) by intersection over union, which
# casts up-left behind the building (the height field also stands on ground hidden behind the
# building, and the overlap score rewarded that); the user found it unnatural. Don't trust fit.
# Length: the one whose cast shadow has as many pixels as the original's own, on six originals:
# temple 0.28, fort 0.2, barracks 0.4, Small City 0.4, Library and Large City under 0.2; median 0.25.
LIGHT = (-0.25, 0.0)
STEP = 8.0  # world units, the depth mesh's grid
SMOOTH = 3.0  # blur radius, pixels, before the shadow mask is cut at one half: the 8-unit cells and
              # 8-unit height steps made the edges zigzag in steps of several pixels (the user,
              # 2026-10-31: "de ce sunt umbrele zimtate?")


def field(texture, alpha):
    zfile, x0, y0 = build_entries(texture.lower())[0]
    old = mesh_vertices(os.path.join(GAME, "art", zfile))
    span = old[:, :2].max(0) - old[:, :2].min(0)
    solid = (alpha == SOLID) | (alpha == TEAM)
    # the grid reaches well past the mesh: new art can be wider than the old mesh. The depth mesh's
    # heights (the lowest of the cell's centre and back and side corners, rounded down): the raw
    # height over each point also counts ground hidden behind a building as standing under it,
    # and on a 3-unit grid the shadows came out four times too big (French Small City).
    xs, ys, H = heights(solid, x0, y0, old[:, :2].min(0) - 2 * PAD * span, old[:, :2].max(0) + 2 * PAD * span, STEP)
    return x0, y0, xs, ys, H, solid


def cast(f, u, v):
    x0, y0, xs, ys, H, solid = f
    img = Image.new("L", (solid.shape[1], solid.shape[0]))
    d = ImageDraw.Draw(img)
    s = STEP / 2
    for j, y in enumerate(ys):
        for i, x in enumerate(xs):
            h = H[j, i]
            if h <= 0:
                continue
            pts = []
            for t in (0.0, h):
                for dx, dy in ((-s, -s), (s, -s), (s, s), (-s, s)):
                    pts.append(project(x + dx + u * t, y + dy + v * t, 0, x0, y0))
            d.polygon(hull(pts), fill=255)
    img = img.filter(ImageFilter.GaussianBlur(SMOOTH))
    return (np.array(img) >= 128) & ~solid


def hull(pts):
    pts = sorted(set((round(p[0], 2), round(p[1], 2)) for p in pts))
    if len(pts) < 3:
        return pts

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def fit(textures):
    data = []
    for tex in textures:
        a = np.array(Image.open(os.path.join(GAME, "art", tex)).convert("RGBA"))[..., 3]
        data.append((field(tex, a), a == SHADOW))
    best = None
    for u in np.arange(-1.0, 1.01, 0.1):
        for v in np.arange(-1.0, 1.01, 0.1):
            ious = []
            for f, real in data:
                m = cast(f, u, v)
                ious.append((m & real).sum() / max(1, (m | real).sum()))
            score = float(np.mean(ious))
            if best is None or score > best[0]:
                best = (score, round(u, 2), round(v, 2), [round(i, 2) for i in ious])
    print(f"light u {best[1]} v {best[2]}: mean IoU {best[0]:.2f}, per texture {best[3]}")
    return best


def main():
    if sys.argv[1] == "fit":
        fit(sys.argv[2:])
        return
    tex, src, out = sys.argv[2:5]
    u, v = (float(sys.argv[5]), float(sys.argv[6])) if len(sys.argv) > 6 else LIGHT
    px = np.array(Image.open(src).convert("RGBA"))
    px[px[..., 3] == SHADOW] = 0
    sh = cast(field(tex, px[..., 3]), u, v)
    px[sh] = (0, 0, 0, SHADOW)
    Image.fromarray(px).save(out)
    print(f"{out}: {sh.sum()} shadow pixels, light ({u}, {v})")


if __name__ == "__main__":
    main()
