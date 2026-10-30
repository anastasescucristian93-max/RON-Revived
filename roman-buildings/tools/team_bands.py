"""Completes painted red bands as player colour after make_sprite.

make_sprite takes only bright, thick red as player colour, so a painted band along a wall
comes out half done: the sunny face turns to player colour, the face in shadow stays red.
This takes every paint-red pixel (also dark red in shadow) that belongs to a large red or
player-colour area, plus red pieces within `reach` px of one (a band cut by a spear or a
beam end; default 4). Small red things on their own (dates, onions, pots, edge specks) stay
as they are.

    python team_bands.py <sprite_B.tga> <out.png> [min_area] [reach]
"""
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

from make_sprite import SOLID, TEAM


def main(src, out, min_area=40, reach=4):
    a = np.array(Image.open(src).convert("RGBA"))
    hsv = np.array(Image.fromarray(a[..., :3]).convert("HSV")).astype(int)
    h, s, v = hsv[..., 0], hsv[..., 1], hsv[..., 2]
    # paint red, also in shadow; beige plaster and wood are hue 15-25
    red = (a[..., 3] == SOLID) & ((h < 10) | (h > 240)) & (s > 120) & (v > 30)
    lab, n = ndimage.label(red | (a[..., 3] == TEAM), structure=np.ones((3, 3)))
    sizes = ndimage.sum(np.ones_like(lab), lab, range(1, n + 1))
    keep = np.isin(lab, [i + 1 for i, size in enumerate(sizes) if size >= int(min_area)])
    red &= ndimage.binary_dilation(keep, iterations=int(reach))
    r, g, b = (a[..., i].astype(int) for i in range(3))
    grey = np.clip((r * 299 + g * 587 + b * 114) // 1000 * 95 // 70, 40, 140)
    for i in range(3):
        a[..., i] = np.where(red, grey, a[..., i])
    a[..., 3] = np.where(red, TEAM, a[..., 3])
    Image.fromarray(a).save(out)
    print(f"{out}: {int(red.sum())} more player-colour pixels")


if __name__ == "__main__":
    main(*sys.argv[1:])
