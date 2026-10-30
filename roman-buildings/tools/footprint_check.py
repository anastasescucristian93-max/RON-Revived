"""Draws a building's footprint diamond over its sprites, to see whether new art sits on the
ground the way the original does (new art fitted by height can overflow the diamond).

    python footprint_check.py <texture> <out.png> [sprite.tga ...]
    python footprint_check.py --all <out_dir>      every sprite in nation_art.ini

<texture> is the game's file name, e.g. village_0ma.tga. The diamond is the ground extent of the
original zbuffer mesh, placed with the <BUILD> entry's spritexoff / spriteyoff (projection in
depth_mesh.py). The original sprite comes first, then each given sprite, side by side, 2x.

It also prints how far each sprite's lower outline (the lowest building pixel of every column)
reaches past the diamond's two front edges and side corners, and the scale around the diamond's
centre (for rescale_sprite.py) that brings a new sprite back to the original's overflow.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

from depth_mesh import GAME, build_entries, mesh_vertices, repo_file, rules

PAD = 40
TOL = 2  # pixels of overflow a new sprite may have beyond the original's


def diamond(texture):
    zfile, x0, y0 = build_entries(texture)[0]
    v = mesh_vertices(os.path.join(GAME, "art", zfile))
    lo, hi = v[:, :2].min(0), v[:, :2].max(0)

    def P(x, y):
        return x0 + 0.70 * (x - y), y0 - 0.31 * (x + y)

    c = [P(lo[0], lo[1]), P(hi[0], lo[1]), P(hi[0], hi[1]), P(lo[0], hi[1])]
    left, right = min(c, key=lambda p: p[0]), max(c, key=lambda p: p[0])
    front, back = max(c, key=lambda p: p[1]), min(c, key=lambda p: p[1])
    return [left, back, right, front]  # screen order


def overflow(path, corners, s=1.0):
    """Largest distance (px) the sprite's lower outline, scaled by s around the diamond's
    centre, reaches below the front edges or past the side corners."""
    a = np.array(Image.open(path).convert("RGBA"))[..., 3]
    body = (a == 255) | (a == 248)
    cols = np.where(body.any(0))[0]
    if not len(cols):
        return 0.0
    bottom = np.array([np.where(body[:, x])[0].max() for x in cols], float)
    cx = sum(c[0] for c in corners) / 4
    cy = sum(c[1] for c in corners) / 4
    xs, ys = cx + (cols - cx) * s, cy + (bottom - cy) * s
    (lx, ly), _, (rx, ry), (fx, fy) = corners
    edge = np.where(xs <= fx, ly + (xs - lx) * (fy - ly) / (fx - lx), fy + (xs - fx) * (ry - fy) / (rx - fx))
    return float(max((ys - edge).max(), lx - xs.min(), xs.max() - rx))


def inside_error(path, corners, dx, dy):
    """Largest distance (px) the sprite's lower outline, moved by (dx, dy), lies outside the
    diamond: below a front edge, above a back edge or past a side corner."""
    a = np.array(Image.open(path).convert("RGBA"))[..., 3]
    body = (a == 255) | (a == 248)
    cols = np.where(body.any(0))[0]
    bottom = np.array([np.where(body[:, x])[0].max() for x in cols], float) + dy
    xs = cols + dx
    (lx, ly), (bx, by), (rx, ry), (fx, fy) = corners

    def line(x, x0, y0, x1, y1):
        return y0 + (x - x0) * (y1 - y0) / (x1 - x0)

    front = np.where(xs <= fx, line(xs, lx, ly, fx, fy), line(xs, fx, fy, rx, ry))
    back = np.where(xs <= bx, line(xs, lx, ly, bx, by), line(xs, bx, by, rx, ry))
    return float(max((bottom - front).max(), (back - bottom).max(), lx - xs.min(), xs.max() - rx))


def best_shift(path, corners, reach=40):
    """The move (dx, dy) that puts the lower outline most inside the diamond."""
    best = min(((inside_error(path, corners, dx, dy), abs(dx) + abs(dy), dx, dy)
                for dx in range(-reach, reach + 1, 2) for dy in range(-reach, reach + 1, 2)))
    return best[2], best[3], best[0]


def best_scale(path, corners, allowed):
    for s in np.arange(1.0, 0.5, -0.01):
        if overflow(path, corners, s) <= allowed:
            return round(float(s), 2)
    return 0.5


def check_all(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    seen = set()
    for texture, game_rel in rules():
        src = repo_file(game_rel)
        if not src or not build_entries(texture) or src in seen or not os.path.exists(os.path.join(GAME, "art", texture)):
            continue
        seen.add(src)
        corners = diamond(texture)
        orig = overflow(os.path.join(GAME, "art", texture), corners)
        new = overflow(src, corners)
        s = best_scale(src, corners, max(orig, 0) + TOL) if new > max(orig, 0) + TOL else 1.0
        name = os.path.splitext(os.path.basename(game_rel))[0]
        nation = game_rel.replace("/", "\\").split("\\")[1]
        print(f"{nation:15} {name:22} original {orig:6.1f}  new {new:6.1f}  scale {s}")
        panels(texture, [src], os.path.join(out_dir, f"{nation}_{name}.png"), corners)


def panels(texture, sprites, out, corners):
    panels = []
    for f in [os.path.join(GAME, "art", texture)] + sprites:
        im = Image.open(f)
        a = im.getchannel("A")
        body = im.convert("RGBA")
        body.putalpha(a.point(lambda q: 255 if q in (248, 255) else 0))
        p = Image.new("RGBA", (im.size[0] + 2 * PAD, im.size[1] + 2 * PAD), (96, 104, 70, 255))
        p.paste(Image.new("RGBA", im.size, (40, 44, 30, 255)), (PAD, PAD), a.point(lambda q: 255 if q == 145 else 0))
        p.alpha_composite(body, (PAD, PAD))
        ImageDraw.Draw(p).polygon([(x + PAD, y + PAD) for x, y in corners], outline=(255, 0, 255))
        panels.append(p)
    W, H = panels[0].size
    sheet = Image.new("RGBA", (len(panels) * (W + 10), H))
    for i, p in enumerate(panels):
        sheet.paste(p, (i * (W + 10), 0))
    sheet.resize((sheet.size[0] * 2, H * 2)).save(out)


def main(texture, out, sprites):
    corners = diamond(texture)
    cx = sum(c[0] for c in corners) / 4
    cy = sum(c[1] for c in corners) / 4
    print(f"{texture}: diamond {[(round(a), round(b)) for a, b in corners]}, centre ({cx:.0f}, {cy:.0f})")
    orig = overflow(os.path.join(GAME, "art", texture), corners)
    print(f"  original overflow {orig:.1f}")
    for f in sprites:
        print(f"  {f}: overflow {overflow(f, corners):.1f}, scale {best_scale(f, corners, max(orig, 0) + TOL)}")
    panels(texture, sprites, out, corners)


if __name__ == "__main__":
    if sys.argv[1] == "--shift":
        c = diamond(sys.argv[2])
        print("original: outline outside the diamond by", round(inside_error(os.path.join(GAME, "art", sys.argv[2]), c, 0, 0), 1))
        print("sprite:   outline outside the diamond by", round(inside_error(sys.argv[3], c, 0, 0), 1))
        dx, dy, e = best_shift(sys.argv[3], c)
        print(f"best move ({dx}, {dy}): outside by {e:.1f}")
    elif sys.argv[1] == "--all":
        check_all(sys.argv[2])
    else:
        main(sys.argv[1], sys.argv[2], sys.argv[3:])
