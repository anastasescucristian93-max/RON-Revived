r"""Turning roof fans for a finished building sprite: FRAMES copies of the sprite with the blades
inside each fan's opening turned, one full turn over the loop. nation_art.c plays them
("<name>~<frames>:<ms>:loop" rules in roman-buildings\nation_art.ini).

    python fan_frames.py <art>          e.g. dutch

ART lists, per building, the worn sprite, the original it replaces (for the .tga layout), the
output pattern (with %d) and the fans: centre and radii in sprite pixels, measured on the sprite
(an opening on a flat roof is an ellipse about half as tall as it is wide).
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
MOD = os.path.join(HERE, "..")
WRITE_LIKE = os.path.join(ROOT, "roman-buildings", "tools", "write_like.py")
FRAMES = 24
INNER = 0.85  # share of the opening that turns (the rim stays)

ART = {
    # Data Center for the Dutch: five cooling towers on the roof
    "dutch": {
        "sprite": os.path.join(MOD, "work", "data_center_dutch_worn.png"),
        "original": os.path.join(MOD, "original", "versailles_template__original.tga"),
        "out": os.path.join(MOD, "nation_art", "dutch", "data_center_%d.tga"),
        "fans": [(150.0, 111.3), (190.0, 87.5), (231.8, 65.5), (275.8, 41.8), (317.5, 18.3)],
        "radii": (15.5, 7.6),
    },
}


SUB = 4      # samples per pixel and axis; each sample also has its own angle across the frame's
             # step (motion blur), so every frame, the first too, is equally soft: spinning blades


def turn(rgba, fans, rx, ry, angle, step):
    """The blades turned by `angle` to `angle + step` degrees: each pixel inside an opening takes
    the colour from the same radius, that much back, on the circle the ellipse is (the mean of
    SUB x SUB cubic samples, their angles spread over the step). Only building pixels change."""
    out = rgba.copy()
    for cx, cy in fans:
        x0, x1 = int(cx - rx - 2), int(cx + rx + 3)
        y0, y1 = int(cy - ry - 2), int(cy + ry + 3)
        yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        u, v = (xx - cx) / rx, (yy - cy) / ry
        inside = (u * u + v * v < INNER * INNER) & (rgba[y0:y1, x0:x1, 3] == 255)
        acc = np.zeros(xx.shape + (3,), np.float32)
        for i in range(SUB):
            for j in range(SUB):
                a = np.radians(angle + step * ((i * SUB + j) * 7 % (SUB * SUB)) / (SUB * SUB))
                us = (xx + (i + 0.5) / SUB - 0.5 - cx) / rx
                vs = (yy + (j + 0.5) / SUB - 0.5 - cy) / ry
                su = us * np.cos(a) + vs * np.sin(a)
                sv = -us * np.sin(a) + vs * np.cos(a)
                sx, sy = cx + su * rx, cy + sv * ry
                for c in range(3):
                    acc[..., c] += ndimage.map_coordinates(rgba[..., c].astype(np.float32), [sy, sx], order=3)
        acc /= SUB * SUB
        for c in range(3):
            out[y0:y1, x0:x1, c] = np.where(inside, np.clip(acc[..., c], 0, 255), out[y0:y1, x0:x1, c])
    return out


def main(key):
    art = ART[key]
    rgba = np.asarray(Image.open(art["sprite"]).convert("RGBA")).copy()
    os.makedirs(os.path.dirname(art["out"]), exist_ok=True)
    tmp = os.path.join(MOD, "work", f"fan_frame_{key}.png")
    for f in range(FRAMES):
        frame = turn(rgba, art["fans"], *art["radii"], f * 360.0 / FRAMES, 360.0 / FRAMES)
        Image.fromarray(frame, "RGBA").save(tmp)
        subprocess.run([sys.executable, WRITE_LIKE, tmp, art["original"], art["out"] % f], check=True,
                       stdout=subprocess.DEVNULL)
    os.remove(tmp)
    print(f"{key}: {FRAMES} frames, {art['out']}")


if __name__ == "__main__":
    if len(sys.argv) != 2 or sys.argv[1] not in ART:
        sys.exit(__doc__)
    main(sys.argv[1])
