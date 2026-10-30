"""Brightens a finished sprite (from make_sprite.py) to match the game's warm tones. Works on
the building pixels only (alpha 255), after keying, so edges never pick up magenta.

    python brighten_sprite.py <in.tga> <out.tga> [gamma] [warmth]

gamma < 1 lifts dark tones (0.75 = moderate, 0.62 = strong); default 0.75.
warmth: 1 (default) warm and saturated, 0 neutral, negative = less orange.
"""
import sys

from PIL import Image, ImageEnhance

SOLID = 255


def main(src, out, gamma=0.75, warmth=1.0):
    """warmth 1 = the warm, more saturated look; 0 = brightness only, colours unchanged;
    negative values cool the image down (less orange)."""
    im = Image.open(src).convert("RGBA")
    r, g, b, a = im.split()

    def lut(k):
        return [min(255, round(255 * (i / 255) ** gamma * k)) for i in range(256)]

    warm = Image.merge("RGB", (r.point(lut(1 + 0.06 * warmth)), g.point(lut(1.0)), b.point(lut(1 - 0.14 * warmth))))
    warm = ImageEnhance.Color(warm).enhance(1 + 0.15 * warmth)
    rgb = Image.composite(warm, im.convert("RGB"), a.point(lambda v: 255 if v == SOLID else 0))
    rgb.putalpha(a)
    rgb.save(out)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], float(sys.argv[3]) if len(sys.argv) > 3 else 0.75,
         float(sys.argv[4]) if len(sys.argv) > 4 else 1.0)
