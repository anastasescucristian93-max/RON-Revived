"""Replaces a finished sprite's ground shadow (alpha 145) with a shorter one: the same projection
as make_sprite.py (each pixel moved by its height in its column), with the shift scaled down.
make_sprite's shift is fitted to the original's shadow; on tall, dense art (the French Large City,
2026-10-31) it threw long dark streaks and filled the courtyard with black.

    python short_shadow.py <in.png> <out.png> <a> <b>

a, b: shadow shift per pixel of height (make_sprite prints the fitted one, e.g. 0.75 0.4).
"""
import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "roman-buildings", "tools"))
from make_sprite import project_shadow  # noqa: E402

src, out, a, b = sys.argv[1], sys.argv[2], float(sys.argv[3]), float(sys.argv[4])
px = np.array(Image.open(src).convert("RGBA"))
body = np.isin(px[..., 3], (255, 248))
px[px[..., 3] == 145] = 0
shadow = np.array(project_shadow(Image.fromarray((body * 255).astype(np.uint8)), a, b)) > 0
shadow = ndimage.binary_opening(shadow, structure=np.ones((3, 3))) & ~body
px[shadow] = (0, 0, 0, 145)
Image.fromarray(px).save(out)
print(f"{out}: {shadow.sum()} shadow pixels, shift ({a}, {b})")
