"""Finishes the age-8 Auto Plant sprite after make_sprite: removes magenta fringe (the thin
railing and the wire fence let the background through) and paints the player-colour bands the
picture lacks, thin bands like the original's: under the main hall's eaves and along the
annex parapet.

    python fix_autoplant.py <sprite_A.tga> <out.tga>
"""
import sys

from PIL import Image, ImageDraw

SOLID, TEAM = 255, 248
# bands as polygons in sprite pixels (397x297 canvas)
BANDS = [
    [(151, 175), (236, 146), (236, 150), (151, 179)],                     # main hall, right face
    [(236, 153), (256, 162), (256, 166), (236, 157)],                     # annex, left face
    [(256, 162), (326, 166), (326, 170), (256, 166)],                     # annex, front face
    [(326, 166), (394, 130), (394, 134), (326, 170)],                     # annex, right face
]
# the balcony railing on the left end: magenta seen through it came out salmon after color_match
RAILING = (20, 80, 50, 120)


def main(src, out):
    im = Image.open(src).convert("RGBA")
    W, H = im.size
    px = im.load()
    pink = 0
    for y in range(H):
        for x in range(W):
            r, g, b, a = px[x, y]
            if a != SOLID:
                continue
            m = min(r, b) - g  # magenta: red and blue well above green
            in_rail = RAILING[0] <= x < RAILING[2] and RAILING[1] <= y < RAILING[3]
            if in_rail and r - g > 20:
                v = (r + g + b) // 3
                px[x, y] = (v, v, v, a)
                pink += 1
            elif m > 60:
                px[x, y] = (0, 0, 0, 0)
                pink += 1
            elif m > 15:
                v = (r + g + b) // 3
                px[x, y] = (v, v, v, a)
                pink += 1
    mask = Image.new("L", (W, H))
    d = ImageDraw.Draw(mask)
    for poly in BANDS:
        d.polygon(poly, fill=255)
    mk = mask.load()
    n = 0
    for y in range(H):
        for x in range(W):
            r, g, b, a = px[x, y]
            if mk[x, y] and a == SOLID:
                lum = (r * 299 + g * 587 + b * 114) // 1000
                grey = max(45, min(150, lum + 25))
                px[x, y] = (grey, grey, grey, TEAM)
                n += 1
    im.save(out)
    print(f"{out}: magenta pixels fixed {pink}, player-colour pixels {n}")


if __name__ == "__main__":
    main(*sys.argv[1:3])
