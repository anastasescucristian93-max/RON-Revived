r"""The steel frame round the folded interface's minimap (camera-smooth\src\minimap.c), in the
interface's steel style: the same riveted bar as the window frames and the theme's domed bolt
(build_reskin.py), laid along the four edges of the minimap's diamond (the user, 2026-11-08).

    python build_minimap_frame.py <out mod_override folder>

The game's interface draws only upright rectangles (IFaceRenderManager::add_smooth), and the
diamond is not quite symmetric (the minimap is drawn with a 3D camera), so the bar comes as two
diagonal tiles: the horizontal bar sheared to a slope of 1/2, going down to the right
("mm_frame_down") and up to the right ("mm_frame_up"). The DLL lays them end to end along each
edge, stretched a little to the edge's own slope; the tile's ends meet (the bar is seamless). At
the four corners a domed bolt ("mm_frame_plate") covers the joins.

Output: art\revived\minimap_frame.tga, declared in interface.xml by build_menu_theme.py
(tex_lines below; IFACEMAINBASE, where the DLL looks its textures up).
Layout constants the DLL needs (minimap.c): TILE_W x TILE_H tiles, the bar's centre line at
x = 0 is at CENTRE_DOWN (down tile) / CENTRE_UP (up tile) from the tile's top, BAR_H the bar's
height in the tile.
"""
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from build_reskin import bar_tile, with_shadow  # noqa: E402
from build_menu_theme import element  # noqa: E402

ATLAS = "art\\revived\\minimap_frame.tga"
BAR_H = 25            # as the window frames' bar
SLOPE = 0.5
PAD = 4               # columns of bar beyond each end of a tile, for filtering
SHADOW = 3            # the frames' drop shadow offset (down and right), and its blur below
PLATE = 40
TOP = 6               # room above the bar in a tile (the shadow is below)


def long_bar(length):
    t = bar_tile(BAR_H)
    out = Image.new("RGBA", (length, BAR_H))
    for x in range(0, length, t.width):
        out.alpha_composite(t, (x, 0))
    return out, t.width


def tile_width():
    _, tw = long_bar(1)
    n = max(1, round(128 / tw))
    return tw * n


TILE_W = tile_width()
DROP = round(TILE_W * SLOPE)
TILE_H = TOP + BAR_H + DROP + SHADOW + 6
CENTRE_DOWN = TOP + BAR_H / 2              # the centre line's y at x = 0, down tile
CENTRE_UP = TOP + DROP + BAR_H / 2         # up tile


def sheared(sign):
    """Three tiles' worth of bar, sheared by sign * SLOPE, with the frames' shadow; the middle
    tile with PAD columns each side is returned (the shadow there is not cut by the ends)."""
    length = 3 * TILE_W
    strip, _ = long_bar(length)
    h = TOP + BAR_H + int(np.ceil(SLOPE * length)) + SHADOW + 6
    canvas = Image.new("RGBA", (length, h))
    src = np.asarray(strip)
    dst = np.zeros((h, length, 4), np.float64)
    # shear with linear interpolation in y: column x moves down by SLOPE * x (sign +) or up
    for x in range(length):
        off = TOP + (SLOPE * x if sign > 0 else SLOPE * (length - x))
        i = int(np.floor(off))
        f = off - i
        col = src[:, x].astype(np.float64)
        pre = np.concatenate([col[:, :3] * col[:, 3:] / 255, col[:, 3:]], axis=1)
        dst[i:i + BAR_H, x] += pre * (1 - f)
        dst[i + 1:i + 1 + BAR_H, x] += pre * f
    a = dst[..., 3:]
    rgb = np.where(a > 0, dst[..., :3] / np.maximum(a, 1e-6) * 255, 0)
    canvas = Image.fromarray(np.dstack([np.clip(rgb, 0, 255), np.clip(a, 0, 255)]).astype(np.uint8), "RGBA")
    canvas = with_shadow(canvas, off=SHADOW)
    # the middle tile: its own top is where the bar's top is at x = TILE_W, minus TOP
    x0 = TILE_W
    y0 = int(SLOPE * TILE_W) if sign > 0 else int(SLOPE * TILE_W)
    if sign < 0:
        y0 = int(SLOPE * (length - 2 * TILE_W))
    return canvas.crop((x0 - PAD, y0, x0 + TILE_W + PAD, y0 + TILE_H))


def plate():
    """The round domed bolt of the theme (sheet_01 element 23): round, so it suits every corner
    of the diamond (the frames' square corner plate is an L bracket for upright corners)."""
    bolt = element(23).resize((PLATE - 2 * SHADOW, PLATE - 2 * SHADOW), Image.LANCZOS)
    return with_shadow(bolt, off=SHADOW).crop((0, 0, PLATE, PLATE))


# atlas layout: the two tiles (with their pads) one under the other, the plate beside them
DOWN_XY = (0, 0)
UP_XY = (0, TILE_H + 2)
PLATE_XY = (TILE_W + 2 * PAD + 2, 0)


def build():
    w = PLATE_XY[0] + PLATE
    h = max(UP_XY[1] + TILE_H, PLATE)
    size = 1 << (max(w, h) - 1).bit_length()
    atlas = Image.new("RGBA", (size, size))
    atlas.alpha_composite(sheared(+1), DOWN_XY)
    atlas.alpha_composite(sheared(-1), UP_XY)
    atlas.alpha_composite(plate(), PLATE_XY)
    return atlas


def tex_lines():
    return ['<IFACETEXLOAD id="TEX_REVIVED_MMFRAME" file=".\\%s" pixformat="PF_32"/>' % ATLAS,
            f'<IFACETEX name="mm_frame_down" texture="TEX_REVIVED_MMFRAME" '
            f'left="{DOWN_XY[0] + PAD}" top="{DOWN_XY[1]}" width="{TILE_W}" height="{TILE_H}"/>',
            f'<IFACETEX name="mm_frame_up" texture="TEX_REVIVED_MMFRAME" '
            f'left="{UP_XY[0] + PAD}" top="{UP_XY[1]}" width="{TILE_W}" height="{TILE_H}"/>',
            f'<IFACETEX name="mm_frame_plate" texture="TEX_REVIVED_MMFRAME" '
            f'left="{PLATE_XY[0]}" top="{PLATE_XY[1]}" width="{PLATE}" height="{PLATE}"/>']


def preview(atlas, path):
    """The frame laid round a 2:1 diamond as the DLL lays it (tiles at scale k), over grass."""
    k = 0.6
    W, H = 420, 240
    img = Image.new("RGBA", (W, H), (150, 140, 70, 255))
    pts = [(30, 120), (210, 30), (390, 120), (210, 210)]
    from PIL import ImageDraw
    ImageDraw.Draw(img).polygon(pts, fill=(60, 70, 60, 255))
    down = atlas.crop((DOWN_XY[0] + PAD, DOWN_XY[1], DOWN_XY[0] + PAD + TILE_W, DOWN_XY[1] + TILE_H))
    up = atlas.crop((UP_XY[0] + PAD, UP_XY[1], UP_XY[0] + PAD + TILE_W, UP_XY[1] + TILE_H))
    pl = atlas.crop((PLATE_XY[0], PLATE_XY[1], PLATE_XY[0] + PLATE, PLATE_XY[1] + PLATE))
    for a, b in ((0, 1), (1, 2), (3, 2), (0, 3)):
        p, q = pts[a], pts[b]
        dx, dy = q[0] - p[0], q[1] - p[1]
        s = dy / dx
        n = max(1, round(dx / (TILE_W * k)))
        w = dx / n
        ky = 2 * abs(s) * w / TILE_W
        tile, c0 = (down, CENTRE_DOWN) if s > 0 else (up, CENTRE_UP)
        t = tile.resize((max(1, round(w)), max(1, round(TILE_H * ky))), Image.LANCZOS)
        for i in range(n):
            img.alpha_composite(t, (round(p[0] + i * w), round(p[1] + i * s * w - c0 * ky)))
    ps = round(PLATE * k)
    plr = pl.resize((ps, ps), Image.LANCZOS)
    for x, y in pts:
        img.alpha_composite(plr, (round(x - ps / 2), round(y - ps / 2)))
    img.resize((W * 2, H * 2), Image.LANCZOS).save(path)


def main(out):
    from build_menu_theme import write_tga
    atlas = build()
    write_tga(atlas, os.path.join(out, ATLAS))
    preview(atlas, os.path.join(HERE, "..", "preview_minimap_frame.png"))
    print(os.path.join(out, ATLAS), atlas.size, "tile", TILE_W, "x", TILE_H,
          "centre down %.1f up %.1f" % (CENTRE_DOWN, CENTRE_UP))


if __name__ == "__main__":
    main(sys.argv[1])
