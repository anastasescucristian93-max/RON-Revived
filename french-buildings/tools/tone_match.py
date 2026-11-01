"""Gives the game's own sprites the tone of the new French buildings (2026-10-31: the user kept the
game's Granary, Lumber Mill, Smelter, Siege Factory, Stable, Dock, Mine, Woodcutter and Lookout
for now and asked for them in the same tone as the new ones).

A colour transfer in Lab: the building pixels' mean and spread of each channel are moved toward
those of the reference sprites, by STRENGTH (1 = all the way). Player colour (alpha 248), the
shadow (145) and the outline stay as they are, so the art and its animated parts keep their
places.

    python tone_match.py <out_dir> <reference.png|tga> ... -- <original.tga> ...
"""
import os
import sys

import numpy as np
from PIL import Image

STRENGTH = 0.8
SOLID = 255
M = np.array([[0.4124, 0.3576, 0.1805], [0.2126, 0.7152, 0.0722], [0.0193, 0.1192, 0.9505]])
WHITE = np.array([0.95047, 1.0, 1.08883])


def rgb2lab(rgb):
    c = np.where(rgb > 0.04045, ((rgb + 0.055) / 1.055) ** 2.4, rgb / 12.92)
    t = (c @ M.T) / WHITE
    f = np.where(t > 216 / 24389, np.cbrt(t), (24389 / 27 * t + 16) / 116)
    return np.stack([116 * f[..., 1] - 16, 500 * (f[..., 0] - f[..., 1]), 200 * (f[..., 1] - f[..., 2])], -1)


def lab2rgb(lab):
    fy = (lab[..., 0] + 16) / 116
    f = np.stack([fy + lab[..., 1] / 500, fy, fy - lab[..., 2] / 200], -1)
    t = np.where(f ** 3 > 216 / 24389, f ** 3, (116 * f - 16) / (24389 / 27)) * WHITE
    c = np.clip(t @ np.linalg.inv(M).T, 0, 1)
    return np.where(c > 0.0031308, 1.055 * c ** (1 / 2.4) - 0.055, 12.92 * c)


class color:  # noqa: N801  (the two calls the script needs)
    rgb2lab = staticmethod(rgb2lab)
    lab2rgb = staticmethod(lab2rgb)


def lab_of(path):
    a = np.array(Image.open(path).convert("RGBA"))
    m = a[..., 3] == SOLID
    return a, m, color.rgb2lab(a[..., :3] / 255.0)


def main(out_dir, refs, sources):
    ref = np.concatenate([lab[m] for _, m, lab in map(lab_of, refs)])
    rmu, rsd = ref.mean(0), ref.std(0)
    os.makedirs(out_dir, exist_ok=True)
    for src in sources:
        a, m, lab = lab_of(src)
        px = lab[m]
        mu, sd = px.mean(0), px.std(0)
        k = 1 + STRENGTH * (rsd / np.maximum(sd, 1e-3) - 1)
        new = (px - mu) * k + mu + STRENGTH * (rmu - mu)
        lab[m] = new
        rgb = np.clip(color.lab2rgb(lab) * 255, 0, 255).astype(np.uint8)
        a[m, :3] = rgb[m]
        out = os.path.join(out_dir, os.path.splitext(os.path.basename(src))[0] + ".png")
        Image.fromarray(a).save(out)
        print(f"{out}: L {mu[0]:.0f}->{new[:, 0].mean():.0f}, a {mu[1]:.1f}->{new[:, 1].mean():.1f}, b {mu[2]:.1f}->{new[:, 2].mean():.1f}")
    print(f"reference L {rmu[0]:.0f} a {rmu[1]:.1f} b {rmu[2]:.1f}, spread {rsd.round(1)}")


if __name__ == "__main__":
    i = sys.argv.index("--")
    main(sys.argv[1], sys.argv[2:i], sys.argv[i + 1:])
