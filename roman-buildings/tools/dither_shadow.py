"""A soft ground shadow for a finished sprite, from Gemini's picture with and without its shadow.

The game draws every shadow pixel (alpha 145) at one fixed darkness, so a soft, fading shadow cut
at one level is a big hard slab (French Market, 2026-10-31: "ti ai batut joc cu umbra aia"). Here
the shadow's strength (how much darker the shaded picture is) is scaled to the sprite and turned
into a scatter of shadow pixels with an ordered (Bayer) dither: dense where Gemini's shadow is
dark, sparse at its fading edge. At game size the scatter reads as a lighter, softer shadow.

    python dither_shadow.py <plain.jpg> <shaded.jpg> <matte.png> <original.tga> <sprite.png> <out.png> [strength]

<sprite.png> must come from make_sprite_white.py --matte on <plain.jpg> (same fit to the
original). strength scales the density (1 = Gemini's darkness against the game's shadow).
"""
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

SOLID, TEAM, SHADOW = 255, 248, 145
GAME_DARK = 0.43  # how much one shadow pixel darkens the ground (alpha 145 drawn as black at ~110/255)
BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16 + 1 / 32


def main(plain, shaded, matte, original, sprite, out, strength=1.0):
    p = np.asarray(Image.open(plain).convert("L"), float)
    s = np.asarray(Image.open(shaded).convert("L").resize((p.shape[1], p.shape[0])), float)
    soft = np.asarray(Image.open(matte).convert("L").resize((p.shape[1], p.shape[0])), float) / 255
    # darkness of Gemini's shadow on the white (0..1), off the building
    # (the shaded picture is ~2 % darker everywhere and JPEG noise adds a few %: a floor of 0.06,
    # or the whole canvas fills with a faint dot pattern)
    dark = np.clip((p - s) / np.maximum(p, 1) - 0.06, 0, 1) / 0.94 * (soft < 0.5)
    dark = ndimage.gaussian_filter(dark, 3)
    dark[dark < 0.04] = 0

    # the same fit make_sprite_white.py used: the object box scaled to the original's height
    obj = soft >= 0.5
    ys, xs = np.nonzero(obj)
    nb = (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)
    oa = np.asarray(Image.open(original).convert("RGBA").getchannel("A"))
    H, W = oa.shape
    ys, xs = np.nonzero(np.isin(oa, [SOLID, TEAM]))
    ob = (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)
    nw, nh = nb[2] - nb[0], nb[3] - nb[1]
    ow, oh = ob[2] - ob[0], ob[3] - ob[1]
    k = min(oh / nh, (W - 2) / nw)
    tw, th = max(1, round(nw * k)), max(1, round(nh * k))
    ox = min(max(1, ob[0] + (ow - tw) // 2), W - 1 - tw)
    oy = ob[3] - th
    # area-average the darkness down to game size, then place it
    small = Image.fromarray(dark.astype(np.float32)).crop(nb).resize((tw, th), Image.BOX)
    D = np.zeros((H, W), np.float32)
    D[oy:oy + th, ox:ox + tw] = np.asarray(small)

    density = np.clip(D / GAME_DARK * strength, 0, 1)
    thr = np.tile(BAYER, (H // 4 + 1, W // 4 + 1))[:H, :W]
    px = np.array(Image.open(sprite).convert("RGBA"))
    px[px[..., 3] == SHADOW] = 0
    sh = (density > thr) & (px[..., 3] == 0)
    px[sh] = (0, 0, 0, SHADOW)
    Image.fromarray(px).save(out)
    print(f"{out}: {sh.sum()} shadow pixels, strength {strength}")


if __name__ == "__main__":
    main(*sys.argv[1:7], *(float(v) for v in sys.argv[7:8]))
