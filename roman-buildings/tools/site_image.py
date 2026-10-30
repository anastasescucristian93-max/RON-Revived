"""Exports a finished building sprite as a picture for the player guide's galleries
(site/RON_Revived/assets): the building on a transparent background with its shadow at 35 %
black, player colour shown as red, cropped to what is drawn.

    python site_image.py <sprite.tga> <out.png> [<sprite.tga> <out.png> ...]

Run it again after changing a sprite, so the site shows the art that is in the game.
"""
import sys

import numpy as np
from PIL import Image

SOLID, TEAM, SHADOW = 255, 248, 145


def export(src, dst):
    a = np.array(Image.open(src).convert("RGBA"))
    cat = a[..., 3]
    out = np.zeros_like(a)
    out[cat == SHADOW, 3] = 90
    body = (cat == SOLID) | (cat == TEAM)
    out[body, :3] = a[body, :3]
    out[body, 3] = 255
    team = cat == TEAM
    grey = a[team, :3].astype(float).mean(1)
    out[team, 0] = np.clip(grey * 1.1, 0, 255)
    out[team, 1] = grey * 0.19
    out[team, 2] = grey * 0.19
    ys, xs = np.where(out[..., 3] > 0)
    im = Image.fromarray(out).crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))
    im.save(dst)
    print(f"{dst}: {im.size[0]}x{im.size[1]}")


if __name__ == "__main__":
    for i in range(1, len(sys.argv) - 1, 2):
        export(sys.argv[i], sys.argv[i + 1])
