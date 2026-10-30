"""Turns neutral light-grey surfaces (concrete, plaster) below a given row into player colour,
for buildings whose team colour is a wall rather than red cloth.

    python team_region.py <sprite.tga> <out.tga> <from_y> [min_lum] [max_spread] [x,y x,y ...]

from_y is a row of the game-size sprite; pixels above it (a white dome, a grey roof) stay.
The optional polygon (sprite pixels) also stays: grey steel in shadow has the same colour as
concrete in shadow, so a tower standing on a concrete base is cut out by its outline.
"""
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter

from make_sprite import SOLID, TEAM, MIN_TEAM_AREA, drop_specks


def main(src, out, from_y, min_lum=55, max_spread=24, keep=None):
    im = Image.open(src).convert("RGBA")
    W, H = im.size
    px = im.load()
    kept = Image.new("L", (W, H))
    if keep:
        ImageDraw.Draw(kept).polygon(keep, fill=255)
    kp = kept.load()
    mask = Image.new("L", (W, H))
    solid = Image.new("L", (W, H))
    mp, sp = mask.load(), solid.load()
    for y in range(H):
        for x in range(W):
            r, g, b, a = px[x, y]
            if a != SOLID:
                continue
            sp[x, y] = 255
            lum = (r * 299 + g * 587 + b * 114) // 1000
            if y >= from_y and not kp[x, y] and lum >= min_lum and max(r, g, b) - min(r, g, b) <= max_spread:
                mp[x, y] = 255
    # close pores in the concrete texture, but never spill past the building's outline
    mask = ImageChops.multiply(mask.filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.MinFilter(3)), solid)
    mask = ImageChops.subtract(mask, kept)
    mask = drop_specks(mask, MIN_TEAM_AREA)
    mk = mask.load()
    n = 0
    for y in range(H):
        for x in range(W):
            if mk[x, y]:
                r, g, b, _ = px[x, y]
                lum = (r * 299 + g * 587 + b * 114) // 1000
                grey = max(40, min(150, lum * 3 // 4))  # keep the shading: lit top, darker faces
                px[x, y] = (grey, grey, grey, TEAM)
                n += 1
    im.save(out)
    print(f"{out}: player-colour pixels {n}")


if __name__ == "__main__":
    poly = [tuple(map(int, p.split(","))) for p in sys.argv[6:]]
    main(sys.argv[1], sys.argv[2], *map(int, sys.argv[3:6]), keep=poly or None)
