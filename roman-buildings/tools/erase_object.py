"""Erases separate objects from an image on a flat magenta background: every connected
object that lies entirely inside the given box is painted magenta.

    python erase_object.py <in> <out> <x0> <y0> <x1> <y1>    (box in the image's pixels)
"""
import sys
from collections import deque

from PIL import Image


def is_object(p):
    r, g, b = p
    return min(r, b) - g < 120  # not magenta


def main(src, dst, x0, y0, x1, y1):
    im = Image.open(src).convert("RGB")
    W, H = im.size
    px = im.load()
    seen = set()
    erased = 0
    for sy in range(y0, y1):
        for sx in range(x0, x1):
            if (sx, sy) in seen or not is_object(px[sx, sy]):
                continue
            comp, inside = [], True
            q = deque([(sx, sy)])
            seen.add((sx, sy))
            while q:
                x, y = q.popleft()
                comp.append((x, y))
                if not (x0 <= x < x1 and y0 <= y < y1):
                    inside = False
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < W and 0 <= ny < H and (nx, ny) not in seen and is_object(px[nx, ny]):
                        seen.add((nx, ny))
                        q.append((nx, ny))
            if inside:
                for x, y in comp:
                    px[x, y] = (255, 0, 255)
                erased += len(comp)
    # The erased object's anti-aliased edge was a mix with the background (pinkish, not
    # magenta): clear every magenta-tinted pixel left in the box.
    for y in range(y0, y1):
        for x in range(x0, x1):
            r, g, b = px[x, y]
            if min(r, b) - g > 30:
                px[x, y] = (255, 0, 255)
    im.save(dst, quality=98)
    print(f"erased {erased} pixels")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], *(int(v) for v in sys.argv[3:7]))
