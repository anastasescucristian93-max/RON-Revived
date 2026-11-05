r"""Builds the resource panel's digits (camera-smooth\src\hud.c) into one interface atlas.

    python build_hud_digits.py <out mod_override folder>

The game writes the panel's and the counters' numbers with a pixel font of about 7x13 texels;
enlarged (hud_scale) they came out in steps, with a yellowish fringe from the colour hidden in
the font picture's transparent texels. These are drawn from a real font instead: white with a
black outline (the game tints them: white, green, black on the yellow cap row), transparent
texels black, PX_PER_UNIT texels per interface unit (2 per screen pixel at the 1.5 interface
scale, so they are shrunk, never enlarged). build_menu_theme.py declares them in interface.xml,
in the IFACEMAINBASE block, as IFACETEX "hud_char_<n>" (n = index in CHARS), next to the qol
buttons; hud.c finds them there and draws its text with them. "hud_block" is an opaque white
square: respanel.c's background under the population and city counters.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ATLAS = "art\\revived\\hud_digits.tga"
CHARS = "0123456789+-/"  # hud.c's HUD_CHARS, same order
PX_PER_UNIT = 3          # hud.c's HUD_PX_PER_UNIT
BOX = 17 * PX_PER_UNIT   # glyph box height: the game's enlarged digits (13 x 4/3 units)
FONT = r"C:\Windows\Fonts\palab.ttf"  # Palatino Linotype Bold, the market titles' look
SIZE = 58                # digits about 14 units tall
STROKE = 2               # the black outline, about 1 screen pixel (2 looked like a grey fringe on the dark plates)
SIDE = 1                 # spare texels left and right of each glyph's ink
BLOCK = (496, 0, 16)     # x, y, size of the opaque block, IFACETEX "hud_block" (its middle)


def build():
    font = ImageFont.truetype(FONT, SIZE)
    cells = []
    for ch in CHARS:
        img = Image.new("RGBA", (BOX * 2, BOX), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        # baseline so the digits sit centred in the box
        top = font.getbbox("0", anchor="ls")[1]
        base = (BOX - top) // 2
        d.text((BOX // 2, base), ch, font=font, fill=(255, 255, 255, 255), anchor="ls",
               stroke_width=STROKE, stroke_fill=(0, 0, 0, 255))
        l, _, r, _ = img.getbbox()
        cells.append(img.crop((l - SIDE, 0, r + SIDE, BOX)))
    width = sum(c.width for c in cells)
    atlas = Image.new("RGBA", (512, 64), (0, 0, 0, 0))
    assert width <= atlas.width and BOX <= atlas.height, (width, BOX)
    x, rects = 0, []
    for c in cells:
        atlas.paste(c, (x, 0))
        rects.append((x, c.width))
        x += c.width
    # an opaque white block (BLOCK): respanel.c's background under the counters, tinted
    atlas.paste((255, 255, 255, 255), (BLOCK[0], BLOCK[1], BLOCK[0] + BLOCK[2], BLOCK[1] + BLOCK[2]))
    assert x <= BLOCK[0], x
    # transparent texels black: no colour to bleed when the game filters the picture
    px = atlas.load()
    for j in range(atlas.height):
        for i in range(atlas.width):
            if px[i, j][3] == 0:
                px[i, j] = (0, 0, 0, 0)
    return atlas, rects


def tex_lines():
    _, rects = build()
    bx, by, bs = BLOCK
    return [f'<IFACETEX name="hud_char_{n}" texture="TEX_REVIVED_DIGITS" left="{x}" top="0" width="{w}" height="{BOX}"/>'
            for n, (x, w) in enumerate(rects)] + [
        f'<IFACETEX name="hud_block" texture="TEX_REVIVED_DIGITS" left="{bx + 4}" top="{by + 4}" width="{bs - 8}" '
        f'height="{bs - 8}"/>']


def load_line():
    return '<IFACETEXLOAD id="TEX_REVIVED_DIGITS" file=".\\%s" pixformat="PF_32"/>' % ATLAS


def main(out):
    sys.path.insert(0, HERE)
    from build_menu_theme import write_tga
    atlas, _ = build()
    write_tga(atlas, os.path.join(out, ATLAS))
    prev = Image.new("RGBA", atlas.size, (50, 50, 50, 255))
    prev.alpha_composite(atlas)
    prev.resize((prev.width * 2, prev.height * 2), Image.NEAREST).save(
        os.path.join(HERE, "..", "preview_hud_digits.png"))


if __name__ == "__main__":
    main(sys.argv[1])
