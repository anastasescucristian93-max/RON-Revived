"""Matches the colours of building sprites to a reference sprite: per channel, the mean and spread of
the solid pixels (alpha 255) are set to the reference's, so a set of buildings reads as one
(Romania's age 7 cities against its Major City, the user: "cel mai mare este cel corect").

    python match_colours.py <reference.tga> <in.tga> <out.tga> [<in.tga> <out.tga> ...]
"""
import sys

import numpy as np
from PIL import Image


def stats(a):
    p = a[..., :3][a[..., 3] == 255]
    return p.mean(0), p.std(0)


def main(ref, pairs):
    mu_r, sd_r = stats(np.asarray(Image.open(ref).convert("RGBA")).astype(np.float64))
    for src, dst in pairs:
        a = np.asarray(Image.open(src).convert("RGBA")).astype(np.float64)
        m = a[..., 3] == 255
        mu, sd = stats(a)
        a[..., :3][m] = np.clip((a[..., :3][m] - mu) / sd * sd_r + mu_r, 0, 255)
        Image.fromarray(a.astype(np.uint8), "RGBA").save(dst)
        print("%s: shift %s, spread x %s" % (dst, (mu_r - mu).round(1), (sd_r / sd).round(2)))


if __name__ == "__main__":
    args = sys.argv[2:]
    main(sys.argv[1], list(zip(args[::2], args[1::2])))
