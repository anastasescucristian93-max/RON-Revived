"""Puts a waving flag in the player's colour on a pole at the top of a finished building sprite,
as the frames of a looping animation (nation_art.ini: <name>.tga~<frames>:<ms>:loop = ...%d.tga).

The flag is player colour (alpha 248, grey shaded: the game tints it), the pole is solid wood.
A wave runs from the pole to the free end; its slope shades the cloth (rising toward the light,
upper right, is lighter). Drawn 4x larger and reduced, a pixel is cloth when it is mostly covered.

    python flag_frames.py <sprite.png> <out pattern with %d> [frames=12] [pole=20] [length=16]
                          [height=10] [x y]

x y: the foot of the pole; by default the highest building pixel (a roof's apex).
"""
import math
import sys

import numpy as np
from PIL import Image, ImageDraw

SS = 4
TEAM, SOLID = 248, 255
WOOD_DARK, WOOD_LIT, KNOB = (62, 46, 32), (104, 80, 54), (150, 122, 66)


def apex(a):
    ys, xs = np.nonzero((a == SOLID) | (a == TEAM))
    top = ys.min()
    row = xs[ys == top]
    return int(round(row.mean())), int(top)


def frame(base, foot, t, frames, pole, length, height):
    W, H = base.size
    px, py = foot
    out = np.array(base)
    # pole: two pixels wide, shaded side left, lit side right, a bronze knob on top
    for y in range(py - pole, py + 1):
        if 0 <= y < H:
            for dx, c in ((0, WOOD_DARK), (1, WOOD_LIT)):
                if 0 <= px + dx < W and out[y, px + dx, 3] != TEAM:
                    out[y, px + dx] = (*c, SOLID)
    for dx in (0, 1):
        out[py - pole - 1, px + dx] = (*KNOB, SOLID)

    # cloth, 4x
    phase = 2 * math.pi * t / frames
    amp = height * 0.22
    big = Image.new("L", (W * SS, H * SS))
    cov = Image.new("L", (W * SS, H * SS))
    shade, mask = ImageDraw.Draw(big), ImageDraw.Draw(cov)
    nu, nv = 48, 6
    x0, y0 = px + 2, py - pole + 1

    def pt(u, v):
        w = amp * u * math.sin(2 * math.pi * 1.3 * u - phase)
        x = x0 + length * u * (1 - 0.06 * (1 + math.cos(2 * math.pi * 1.3 * u - phase)) * u)
        y = y0 + height * v + w + 1.5 * u * u  # the free end droops a little
        return x * SS, y * SS

    for i in range(nu):
        u0, u1 = i / nu, (i + 1) / nu
        um = (u0 + u1) / 2
        slope = amp * (math.sin(2 * math.pi * 1.3 * um - phase)
                       + um * 2 * math.pi * 1.3 * math.cos(2 * math.pi * 1.3 * um - phase))
        g = int(max(55, min(165, 112 - 9 * slope)))
        for j in range(nv):
            v0, v1 = j / nv, (j + 1) / nv
            quad = [pt(u0, v0), pt(u1, v0), pt(u1, v1), pt(u0, v1)]
            shade.polygon(quad, fill=g - 6 * j // nv)
            mask.polygon(quad, fill=255)
    g = np.asarray(big.resize((W, H), Image.BOX), np.float32)
    c = np.asarray(cov.resize((W, H), Image.BOX), np.float32)
    cloth = c > 115
    grey = np.clip(g[cloth] * 255 / np.maximum(c[cloth], 1), 0, 255).astype(np.uint8)
    out[cloth, 0] = out[cloth, 1] = out[cloth, 2] = grey
    out[cloth, 3] = TEAM
    return Image.fromarray(out)


def main():
    src, pattern = sys.argv[1], sys.argv[2]
    args = [int(v) for v in sys.argv[3:]]
    frames, pole, length, height = (args + [12, 20, 16, 10][len(args):])[:4]
    base = Image.open(src).convert("RGBA")
    foot = tuple(args[4:6]) if len(args) >= 6 else apex(np.asarray(base)[..., 3])
    if foot[1] - pole - 1 < 0:
        sys.exit(f"no room for a {pole}-pixel pole above {foot}: scale the sprite down first")
    for t in range(frames):
        frame(base, foot, t, frames, pole, length, height).save(pattern % t)
    print(f"{frames} frames, pole at {foot}, {pole} px, flag {length}x{height}")


if __name__ == "__main__":
    main()
