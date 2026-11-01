"""Keeps the ground shadow Gemini drew on the magenta, as the game's shadow (alpha 145), on a sprite
made by make_sprite.py from the same picture. The user liked Gemini's shadow on the French Market
(2026-10-31) after every shadow made in code looked wrong.

    python gemini_shadow.py <gemini.jpg> <cleaned.png> <original.tga> <sprite.tga|png> <out.png>

<gemini.jpg> is the picture as downloaded (the shadow is still in it); <cleaned.png> the same after
clean_background.py, which make_sprite.py read. Gemini's shadow is the background's magenta, only
darker. It is found in the raw picture, then scaled and placed exactly as make_sprite placed the
building: the same fit to the original's height, centred, standing on the same bottom.
"""
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

import make_sprite as ms

DARKER = 0.85  # a shadow pixel is at most this bright against the background's red + blue


def main(raw_path, clean_path, original, sprite_path, out):
    orig = Image.open(original).convert("RGBA")
    W, H = orig.size
    oa = orig.getchannel("A")

    clean = Image.open(clean_path).convert("RGB")
    ms.MAGENTA = ms.background(clean)
    a_big = ms.key_alpha(clean)
    nb = a_big.point(lambda v: 255 if v >= 128 else 0).getbbox()
    ob = oa.point(lambda v: 255 if v in (ms.SOLID, ms.TEAM) else 0).getbbox()
    nw, nh = nb[2] - nb[0], nb[3] - nb[1]
    ow, oh = ob[2] - ob[0], ob[3] - ob[1]
    s = min(oh / nh, (W - 2) / nw)  # make_sprite.main's fit
    tw, th = max(1, round(nw * s)), max(1, round(nh * s))
    ox = min(max(1, ob[0] + (ow - tw) // 2), W - 1 - tw)
    oy = ob[3] - th

    raw = np.array(Image.open(raw_path).convert("RGB")).astype(int)
    bg = np.array(ms.MAGENTA)
    m = np.minimum(raw[..., 0], raw[..., 2]) - raw[..., 1]  # still magenta in hue
    rb = (raw[..., 0] + raw[..., 2]) / 2
    shadow = (m > 40) & (rb < DARKER * (bg[0] + bg[2]) / 2) & (np.array(a_big) < 128)
    shadow = ndimage.binary_opening(shadow, np.ones((5, 5)))
    shadow = ndimage.binary_fill_holes(shadow)
    sx, sy = tw / nw, th / nh
    big = Image.fromarray((shadow * 255).astype(np.uint8))
    small = big.transform((W, H), Image.AFFINE, (1 / sx, 0, nb[0] - ox / sx, 0, 1 / sy, nb[1] - oy / sy), Image.BILINEAR)
    sh = np.array(small) >= 128

    px = np.array(Image.open(sprite_path).convert("RGBA"))
    px[px[..., 3] == ms.SHADOW] = 0
    sh &= px[..., 3] == 0
    px[sh] = (0, 0, 0, ms.SHADOW)
    Image.fromarray(px).save(out)
    print(f"{out}: {sh.sum()} shadow pixels from Gemini's shadow")


if __name__ == "__main__":
    main(*sys.argv[1:6])
