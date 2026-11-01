"""Turns a building picture on a plain white (or light grey) background into a game sprite,
without the magenta keying of make_sprite.py, which left dark specks and pink edges.

The background is found by its shape: it is the region of near-background colour that touches
the picture's border, plus large enclosed patches of the same colour (a courtyard). Light walls
inside the building stay. The outline is pulled in by one pixel at picture size, so no white
fringe is left. The colours are not changed: no colour matching, no wear (the user, 2026-10-28).

    python make_sprite_white.py <picture> <original.tga> <out.tga> [--shadow | --keep-shadow] [--tol N]

--matte m.png (from rmbg_matte.py, the AI cut-out) replaces the colour keying: the outline follows
the matte, the white is taken out of the edge colours, no pixel is pulled in. With it,
--shadow-pic shaded.jpg (the same picture with Gemini's shadow; <picture> is the one without)
gives the shadow as the difference of the two. This is the way since 2026-10-31: the keyed
outlines came out ragged ("ciopartit").

--keep-shadow keeps the soft grey shadow Gemini drew on the white as the game's shadow (alpha 145),
scaled and placed with the building (the French Market, 2026-10-31: the user liked it). Without
it that shadow, being darker than the background, would be keyed as building.

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


def gemini_shadow(rgb, bgmask, bg):
    """The soft grey shadow Gemini draws on the white: smooth (little texture), neutral grey (the
    French Market's is (128-131) on all three channels), not dark, darker than the background, and
    joined to it. Dark posts and barrels (luminance about 40, a little warm) and textured stone
    stay building: a looser test took the market hall's posts for shadow."""
    a = rgb.astype(float)
    lum = a.mean(axis=2)
    chroma = a.max(axis=2) - a.min(axis=2)
    mean = ndimage.uniform_filter(lum, 5)
    std = np.sqrt(np.maximum(ndimage.uniform_filter(lum * lum, 5) - mean * mean, 0))
    cand = (chroma < 8) & (std < 10) & (lum > 100) & (lum < bg.mean() - 6) & ~bgmask
    lab, n = ndimage.label(cand)
    # the soft rim between shadow and background has gradients: reach across it
    touch = set(np.unique(lab[ndimage.binary_dilation(bgmask, np.ones((15, 15)))])) - {0}
    sh = np.isin(lab, list(touch))
    # shaded ground Gemini leaves enclosed among the props (the floor under the market hall, the same
    # neutral grey as the shadow but cut off from the white): big, smooth and colourless: shadow too
    light = (chroma < 14) & (std < 6) & (lum > 100) & ~bgmask & ~sh
    lab, n = ndimage.label(light)
    if n:
        sizes = ndimage.sum(light, lab, range(1, n + 1))
        sh |= np.isin(lab, [i + 1 for i, s in enumerate(sizes) if s >= 300])
    sh = ndimage.binary_opening(sh, np.ones((3, 3)))
    # a smooth outline: at game size the cut is one bit, so a ragged mask turns into a zigzag
    return ndimage.gaussian_filter(sh.astype(float), 4) >= 0.5


def shadow_rim(rgb, bgmask, sh):
    """The light, nearly colourless rim between Gemini's shadow and the white (a soft gradient, too
    dark for the background test, too textured for the shadow test): joined to either, it is
    background. Left in, it was a thin white outline round the shadow."""
    a = rgb.astype(float)
    lum = a.mean(axis=2)
    chroma = a.max(axis=2) - a.min(axis=2)
    rim = (chroma < 14) & (lum > 150) & ~bgmask & ~sh
    lab, n = ndimage.label(rim)
    touch = set(np.unique(lab[ndimage.binary_dilation(bgmask | sh, np.ones((3, 3)))])) - {0}
    return np.isin(lab, list(touch))


def shadow_from_pair(plain, shaded, soft):
    """The shadow as the difference between the same picture with and without it (the user asks
    Gemini for both, 2026-10-31): where the shaded one is darker, off the building. Smoothed, so the
    one-bit cut at game size has a clean edge."""
    d = plain.astype(float).mean(axis=2) - np.asarray(Image.open(shaded).convert("RGB").resize(
        (plain.shape[1], plain.shape[0])), float).mean(axis=2)
    sh = (ndimage.gaussian_filter(d, 2) > 12) & (soft < 0.25)
    sh = ndimage.binary_opening(sh, np.ones((5, 5)))
    return ndimage.gaussian_filter(sh.astype(float), 4) >= 0.5


def main(src, original, out, shadow=False, tol=24, keep_shadow=False, matte=None, shadow_pic=None):
    pic = np.asarray(Image.open(src).convert("RGB"))
    bgmask, bg = background_mask(pic, tol)
    gsh = None
    soft = None
    if matte:  # an AI matte (rmbg_matte.py): the real outline, soft at the edge
        soft = np.asarray(Image.open(matte).convert("L").resize((pic.shape[1], pic.shape[0])), np.float32) / 255
        obj = soft >= 0.5
        if shadow_pic:
            gsh = shadow_from_pair(pic, shadow_pic, soft)
    else:
        if keep_shadow:  # the shadow is not building: take it out before the outline is found
            gsh = gemini_shadow(pic, bgmask, bg)
            bgmask = bgmask | gsh | shadow_rim(pic, bgmask, gsh)
        obj = ~bgmask
    # drop specks (watermark, JPEG noise), keep the building and small props
    lab, n = ndimage.label(obj)
    sizes = ndimage.sum(obj, lab, range(1, n + 1))
    obj = np.isin(lab, [i + 1 for i, s in enumerate(sizes) if s >= 40])
    if soft is None:
        obj = ndimage.binary_erosion(obj, structure=np.ones((3, 3)))  # no white fringe
        weight = obj.astype(np.float32)
    else:
        # the matte's soft edge is a blend with the white: take the white out of the edge colours
        # (c = (p - (1 - a) bg) / a), then downscale weighted by the matte and cut at one half
        weight = soft * obj
        a3 = np.maximum(weight, 0.05)[..., None]
        pic = np.clip((pic - (1 - a3) * bg) / a3, 0, 255).astype(np.uint8)

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
    alpha = Image.fromarray((weight * 255).astype(np.uint8)).crop(nb).resize((tw, th), Image.LANCZOS)
    pre = (pic * weight[..., None]).astype(np.uint8)
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
    if gsh is not None:  # Gemini's own shadow, scaled and placed like the building
        small = Image.fromarray((gsh * 255).astype(np.uint8)).transform(
            (W, H), Image.AFFINE, (nw / tw, 0, nb[0] - ox * nw / tw, 0, nh / th, nb[1] - oy * nh / th), Image.BILINEAR)
        sh = np.asarray(small) >= 128
        # the building's outline was pulled in by a pixel and the shadow stops at the old one:
        # close the 1-2 px gap, or the ground shows through as a light ring round every prop
        sh |= ndimage.binary_dilation(sh, iterations=2) & ndimage.binary_dilation(body, iterations=2)
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
    keep = "--keep-shadow" in args
    if keep:
        args.remove("--keep-shadow")
    tol = 24
    if "--tol" in args:
        i = args.index("--tol")
        tol = int(args[i + 1])
        del args[i:i + 2]
    opt = {}
    for flag in ("--matte", "--shadow-pic"):
        if flag in args:
            i = args.index(flag)
            opt[flag[2:].replace("-", "_")] = args[i + 1]
            del args[i:i + 2]
    main(*args[:3], shadow=sh, tol=tol, keep_shadow=keep, **opt)
