r"""Cuts each nation medallion out of the Gemini sheets as a square with its background, for
cleaning by hand (background removed, saved as PNG with alpha).

    python export_flag_squares.py <out folder>

Writes <out folder>\<nation>.png, one per medallion of SHEETS in build_flags.py, at the
sheet's own resolution, with a margin around the gold rim. Cleaned files go back into
source\flags\cut\ under the same names; build_flags.py uses them instead of its own cut.
"""
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import build_flags as bf

MARGIN = 0.10  # of the radius, around the rim


def main(out):
    os.makedirs(out, exist_ok=True)
    n = 0
    for name, keys in bf.SHEETS.items():
        img = Image.open(os.path.join(bf.SRC, name)).convert("RGB")
        w, h = img.size
        rows = (len(keys) + 2) // 3
        for k, key in enumerate(keys):
            if key is None:
                continue
            col, row = k % 3, k // 3
            box = (col * w // 3, row * h // rows, (col + 1) * w // 3, (row + 1) * h // rows)
            cx, cy, r = bf.find_medallion(img.crop(box))
            cx, cy, r = box[0] + cx, box[1] + cy, r * (1 + MARGIN)
            img.crop((round(cx - r), round(cy - r), round(cx + r), round(cy + r))).save(
                os.path.join(out, key + ".png"))
            n += 1
    print(n, "squares in", out)


if __name__ == "__main__":
    main(sys.argv[1])
