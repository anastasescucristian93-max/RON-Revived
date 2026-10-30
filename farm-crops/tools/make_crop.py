"""Builds a farm crop texture in the layout of the game's farm_wheat_1.tga from a Gemini strip.

    python make_crop.py <strip.jpg> <farm_wheat_1.tga> <out.png> [top_row]

farm_wheat_1.tga (128x128) holds two 64x64 tiles on top (stalk tips on a diagonal) and a
128x64 band below (dense crop, thin stems at the bottom edge). The new texture keeps the
wheat's alpha, so the shapes stay where the engine expects them, and takes its colours
from the opaque part of the strip (rows from top_row down, default: the first row that is
at least 98% plant).
"""
import sys

import numpy as np
from PIL import Image, ImageFilter


def is_magenta(rgb):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    return (r > 110) & (b > 110) & (g < r - 40) & (g < b - 40)


def plant_rows(rgb):
    cover = 1 - is_magenta(rgb).mean(axis=1)
    return int(np.argmax(cover >= 0.999))


def fill_magenta(im):
    """Paints leftover background pixels with the colour of the plants around them."""
    a = np.asarray(im).copy()
    bad = is_magenta(a.astype(int))
    for _ in range(8):
        if not bad.any():
            break
        blur = np.asarray(Image.fromarray(np.where(bad[..., None], 0, a)).filter(ImageFilter.BoxBlur(3))).astype(float)
        weight = np.asarray(Image.fromarray((~bad * 255).astype(np.uint8)).filter(ImageFilter.BoxBlur(3))).astype(float)
        ok = bad & (weight > 0)
        a[ok] = (blur[ok] * 255 / weight[ok][:, None]).clip(0, 255).astype(np.uint8)
        bad &= ~ok
    return Image.fromarray(a)


def seamless(im, blend):
    """Cross-fades the right edge into the left one so the image repeats horizontally."""
    a = np.asarray(im).astype(float)
    w = a.shape[1] - blend
    out = a[:, :w].copy()
    t = np.linspace(0, 1, blend)[None, :, None]
    out[:, :blend] = a[:, w:] * (1 - t) + a[:, :blend] * t
    return Image.fromarray(out.astype(np.uint8))


def main(strip, wheat, out, top_row=None):
    src = Image.open(strip).convert("RGB")
    rgb = np.asarray(src).astype(int)
    top = int(top_row) if top_row is not None else plant_rows(rgb)
    body = fill_magenta(src.crop((0, top, src.width, src.height)))
    h = body.height

    tex = Image.new("RGB", (128, 128))
    # Band: about two plant widths per 64 px, same pixel aspect as the source.
    band = seamless(body.crop((0, 0, 2 * h + 24, h)), 24).resize((128, 64), Image.LANCZOS)
    tex.paste(band, (0, 64))
    # Top tiles: two different square crops.
    for i, x in enumerate((h, 3 * h)):
        tile = body.crop((x, 0, x + h, h)).resize((64, 64), Image.LANCZOS)
        tex.paste(tile, (64 * i, 0))
    tex = tex.filter(ImageFilter.UnsharpMask(radius=1, percent=60, threshold=2))

    alpha = Image.open(wheat).convert("RGBA").getchannel("A")
    tex.putalpha(alpha)
    tex.save(out)
    print(f"{out}: colours from rows {top}..{src.height} of {strip}")


if __name__ == "__main__":
    main(*sys.argv[1:])
