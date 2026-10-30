"""Removes the black specks along a building sprite's outline: Gemini draws a dark contour and
contact shadow around its buildings, and make_sprite keeps it as near-black pixels and small
spikes on the edge, which show as dirt in game (the user, 2026-10-28).

Spikes (dark outline pixels with at most `spike` building neighbours) become empty, or shadow
where they touch the shadow; the other dark outline pixels take the median colour of the
non-dark building pixels around them. Shadow, player colour and the rest stay as they are.

    python clean_outline.py <sprite.tga|png> <out.png> [dark=60] [passes=3]
"""
import sys

import numpy as np
from PIL import Image

SOLID, TEAM, SHADOW = 255, 248, 145


def lum(c):
    return (c[..., 0] * 299 + c[..., 1] * 587 + c[..., 2] * 114) // 1000


def main(src, out, dark=60, passes=3, spike=3):
    a = np.asarray(Image.open(src).convert("RGBA")).astype(int)
    H, W, _ = a.shape
    fixed = removed = 0
    for _ in range(passes):
        body = np.isin(a[..., 3], [SOLID, TEAM])
        L = lum(a)
        isdark = body & (L < dark) & (a[..., 3] == SOLID)
        new = a.copy()
        for y, x in zip(*np.nonzero(isdark)):
            y0, y1, x0, x1 = max(0, y - 1), min(H, y + 2), max(0, x - 1), min(W, x + 2)
            nb = body[y0:y1, x0:x1]
            if nb.all():
                continue  # inside the building, not on the outline
            if nb.sum() - 1 <= spike:
                shadow_near = (a[y0:y1, x0:x1, 3] == SHADOW).any()
                new[y, x] = (0, 0, 0, SHADOW) if shadow_near else (0, 0, 0, 0)
                removed += 1
                continue
            Y0, Y1, X0, X1 = max(0, y - 2), min(H, y + 3), max(0, x - 2), min(W, x + 3)
            win = a[Y0:Y1, X0:X1]
            ok = (win[..., 3] == SOLID) & (lum(win) >= dark)
            if ok.sum() >= 2:
                new[y, x, :3] = np.median(win[ok][:, :3], axis=0).astype(int)
                fixed += 1
        a = new
    Image.fromarray(a.astype(np.uint8)).save(out)
    print(f"{out}: {fixed} outline pixels recoloured, {removed} spikes removed")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], *map(int, sys.argv[3:5]))
