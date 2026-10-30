"""Turns a building picture on a plain white (or light grey) background into a game sprite,
without the magenta keying of make_sprite.py, which left dark specks and pink edges.

The background is found by its shape: it is the region of near-background colour that touches
the picture's border, plus large enclosed patches of the same colour (a courtyard). Light walls
inside the building stay. The outline is pulled in by one pixel at picture size, so no white
fringe is left. The colours are not changed: no colour matching, no wear (the user, 2026-10-28).

    python make_sprite_white.py <picture> <original.tga> <out.tga> [--shadow] [--tol N]

The picture is fitted like make_sprite does it: the original's height, centred, standing on the
same bottom. With --shadow the ground shadow is projected and fitted to the original's own
shadow (make_sprite.project_shadow). Red stays red; player colour is a later step.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_sprite import project_shadow  # noqa: E402

SOLID, TEAM, SHADOW = 255, 248, 145


def background_mask(rgb, tol):
    a = rgb.astype(int)
    H, W, _ = a.shape
    border = np.concatenate([a[0], a[-1], a[:, 0], a[:, -1]])
    bg = np.median(border, axis=0)
    diff = np.abs(a - bg).max(axis=2)
    near = diff <= tol
    lab, n = ndimage.label(near)
    edge = set(np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]]))) - {0}
    mask = np.isin(lab, list(edge))
    # enclosed patches of plain background (a courtyard, a gap under a bridge): only the
    # flattest background colour, so light walls and laundry, which have texture, stay
    tight = diff <= 8
    lab2, n2 = ndimage.label(tight & ~mask)
    if n2:
        sizes = ndimage.sum(np.ones_like(lab2), lab2, range(1, n2 + 1))
        big = [i + 1 for i, s in enumerate(sizes) if s >= 30]
        mask |= np.isin(lab2, big)
    return mask, bg


def main(src, original, out, shadow=False, tol=24):
    pic = np.asarray(Image.open(src).convert("RGB"))
    bgmask, bg = background_mask(pic, tol)
    obj = ~bgmask
    # drop specks (watermark, JPEG noise), keep the building and small props
    lab, n = ndimage.label(obj)
    sizes = ndimage.sum(obj, lab, range(1, n + 1))
    obj = np.isin(lab, [i + 1 for i, s in enumerate(sizes) if s >= 40])
    obj = ndimage.binary_erosion(obj, structure=np.ones((3, 3)))  # no white fringe

    orig = Image.open(original).convert("RGBA")
    W, H = orig.size
    oa = np.asarray(orig.getchannel("A"))
    ys, xs = np.nonzero(np.isin(oa, [SOLID, TEAM]))
    ob = (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)
    ys, xs = np.nonzero(obj)
    nb = (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)
    nw, nh = nb[2] - nb[0], nb[3] - nb[1]
    ow, oh = ob[2] - ob[0], ob[3] - ob[1]
    s = min(oh / nh, (W - 2) / nw)
    tw, th = max(1, round(nw * s)), max(1, round(nh * s))
    ox = min(max(1, ob[0] + (ow - tw) // 2), W - 1 - tw)
    oy = ob[3] - th

    # premultiplied downscale, so the edge keeps the building's colour
    alpha = Image.fromarray((obj * 255).astype(np.uint8)).crop(nb).resize((tw, th), Image.LANCZOS)
    pre = (pic * obj[..., None]).astype(np.uint8)
    pre = Image.fromarray(pre).crop(nb).resize((tw, th), Image.LANCZOS)
    A = np.zeros((H, W), np.float32)
    P = np.zeros((H, W, 3), np.float32)
    A[oy:oy + th, ox:ox + tw] = np.asarray(alpha, np.float32)
    P[oy:oy + th, ox:ox + tw] = np.asarray(pre, np.float32)
    body = A >= 128
    rgb = np.clip(P * 255 / np.maximum(A, 1)[..., None], 0, 255).astype(np.uint8)

    outa = np.zeros((H, W, 4), np.uint8)
    if shadow:
        osolid = Image.fromarray((np.isin(oa, [SOLID, TEAM]) * 255).astype(np.uint8))
        oshadow = oa == SHADOW
        best, best_score = (0.3, 0.0), -1.0
        for ai in range(31):
            for bi in range(-8, 9):
                proj = np.asarray(project_shadow(osolid, ai * 0.05, bi * 0.05)) > 0
                union = (proj | oshadow).sum()
                score = (proj & oshadow).sum() / union if union else 0
                if score > best_score:
                    best, best_score = (ai * 0.05, bi * 0.05), score
        sh = np.asarray(project_shadow(Image.fromarray((body * 255).astype(np.uint8)), *best)) > 0
        outa[sh & ~body] = (0, 0, 0, SHADOW)
    outa[body, :3] = rgb[body]
    outa[body, 3] = SOLID
    Image.fromarray(outa).save(out)
    print(f"{out}: {W}x{H}, background {tuple(int(v) for v in bg)}, scale {s:.3f}")


if __name__ == "__main__":
    args = sys.argv[1:]
    sh = "--shadow" in args
    if sh:
        args.remove("--shadow")
    tol = 24
    if "--tol" in args:
        i = args.index("--tol")
        tol = int(args[i + 1])
        del args[i:i + 2]
    main(*args[:3], shadow=sh, tol=tol)
