"""Gives a set of building sprites one colour grade, for buildings meant to read as one style
(Romania's communist age 7-8 set: every Gemini picture came with its own cast, some warm and
brown, some dark, some flat; the user, 2026-10-29).

    python grade_sprite.py <in.tga> <out.tga> [lum=105] [contrast=46] [chroma=16]

Only alpha 255 pixels change (player colour 248 and shadow 145 stay):
  1. the colour cast of the greys is taken out: the mean tint of the near-grey pixels (walls,
     concrete, roofs) is subtracted, weighted by how grey each pixel is, so painted details keep
     their colour;
  2. the saturation is scaled so the mean chroma is at most <chroma>;
  3. luminance mean and spread are matched to <lum> / <contrast> (measured on the age 7 Major City
     and University, the two that looked right).
"""
import sys

import numpy as np
from PIL import Image


def grade(a, lum_t=105.0, con_t=46.0, chroma_t=16.0):
    a = a.astype(np.float64)
    m = a[..., 3] == 255
    rgb = a[..., :3][m]
    sat = rgb.max(1) - rgb.min(1)
    lum = rgb.mean(1)
    greys = rgb[(sat < 40) & (lum > 40)]
    tint = greys.mean(0) - greys.mean()
    w = np.clip(1 - sat / 60.0, 0, 1)[:, None]
    rgb = rgb - tint * w
    lum = rgb.mean(1, keepdims=True)
    chroma = (rgb.max(1) - rgb.min(1)).mean()
    k = min(1.0, chroma_t / max(chroma, 1e-6))
    rgb = lum + (rgb - lum) * k
    lum = rgb.mean(1, keepdims=True)
    rgb = rgb + (lum_t + (lum - lum.mean()) * (con_t / lum.std()) - lum)
    out = a.copy()
    out[..., :3][m] = np.clip(rgb, 0, 255)
    return out.astype(np.uint8), tint, chroma, k


if __name__ == "__main__":
    src, dst = sys.argv[1:3]
    args = [float(x) for x in sys.argv[3:6]]
    out, tint, chroma, k = grade(np.asarray(Image.open(src).convert("RGBA")), *args)
    Image.fromarray(out, "RGBA").save(dst)
    print("%s: cast R%+.1f G%+.1f B%+.1f removed, chroma %.1f x%.2f" % (dst, *tint, chroma, k))
