"""Brings a soft, flat building sprite up to the game's own: contrast and sharpness of the solid
pixels matched to targets measured on the game's city sprites (city_6, village_6,
university_6: luminance std about 48, mean |laplacian| about 48). For Gemini pictures that look
hazy next to the game's buildings (Romania's Large City 7, 2026-10-29: std 37, sharpness 25).

    python crisp_sprite.py <in.tga> <out.tga> [contrast std=48] [sharpness=46]

Only alpha 255 pixels change (player colour 248 and shadow 145 stay as they are). Contrast: each
colour is spread around the sprite's mean luminance by std_target / std. Sharpness: unsharp
masks of growing strength until the mean |laplacian| reaches the target; the background is
filled with the nearest building colour first, so the outline gets no dark or light halo.
"""
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

SOLID = 255


def sharpness(lum, mask):
    return np.abs(ndimage.laplace(lum))[mask].mean()


def main(src, dst, std_target=48.0, sharp_target=46.0):
    a = np.asarray(Image.open(src).convert("RGBA")).astype(np.float64)
    solid = a[..., 3] == SOLID
    rgb = a[..., :3].copy()
    lum = rgb.mean(axis=2)
    mean, std = lum[solid].mean(), lum[solid].std()
    k = std_target / std
    rgb[solid] = mean + (rgb[solid] - mean) * k
    rgb = np.clip(rgb, 0, 255)
    # the background takes the nearest building colour, so blurring does not pull in black
    _, (iy, ix) = ndimage.distance_transform_edt(~solid, return_indices=True)
    filled = rgb[iy, ix]
    before = sharpness(rgb.mean(axis=2), solid)
    out = filled
    for amount in np.arange(0.2, 3.01, 0.1):
        blur = ndimage.gaussian_filter(filled, sigma=(1.0, 1.0, 0))
        out = np.clip(filled + amount * (filled - blur), 0, 255)
        if sharpness(out.mean(axis=2), solid) >= sharp_target:
            break
    res = a.copy()
    res[solid, :3] = out[solid]
    Image.fromarray(res.astype(np.uint8), "RGBA").save(dst)
    print("%s: contrast std %.1f -> %.1f (x%.2f), sharpness %.1f -> %.1f (unsharp %.1f)" % (
        dst, std, out.mean(axis=2)[solid].std(), k, before, sharpness(out.mean(axis=2), solid), amount))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], *map(float, sys.argv[3:5]))
