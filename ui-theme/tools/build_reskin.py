r"""Repaints the game's shared interface atlases in the steel style, piece for piece in place,
so every window that uses them (game setup, options, dialogs...) changes without editing its
XML. Written to <out>\art\ under the same names; the game's files are only read.

    python build_reskin.py <game folder> <out mod_override folder>

  wrapper_pieces_01.tga        frames (IFACEBORDER pieces), section bars, the frame around
                               the Start / Back buttons: wood becomes steel
  wrapper_pieces_02.tga        buttons, tabs, check boxes, drop-down fields and arrows: glossy
  Wrapper_pieces_gamespy.tga   blue becomes gunmetal, white fields become light steel

The frame pieces are drawn new (a riveted steel bar, see ELEMENTS); everything else is
recoloured, keeping the game's shading. Piece positions are the game's (see ENGINE_NOTES.md).
"""
import colorsys
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

from build_menu_theme import element, horizontal_tile, read_tga, write_tga

HERE = os.path.dirname(os.path.abspath(__file__))
PREVIEW = os.path.join(HERE, "..", "preview_reskin")

STEEL = np.array([0.93, 0.97, 1.03])   # cool grey tint
GUNMETAL = np.array([0.90, 0.95, 1.00])
FIELD = np.array([0.84, 0.87, 0.90])   # white drop-down fields -> light steel
ACCENT = np.array([1.50, 0.886, 0.24])  # the theme's orange (255,150,40) per unit of luminance
# Yellow becomes the orange accent only where it marks a selected state (the selected-item
# pills); elsewhere (the gold rims of every button) it becomes steel like the wood, so the
# accent stays rare and means "selected".
ACCENT_02 = [(240, 135, 400, 205)]
# Only the interface chrome is recoloured; pictures and status icons keep their colours.
PROTECT_02 = [(147, 0, 193, 68)]                       # ready / not ready hands
GAMESPY_CHROME = (205, 0, 512, 305)                    # the lobby pieces (to the right edge: the frame ends at 512); globes, hand... stay
PROTECT_GAMESPY = [(225, 150, 295, 172)]               # bells


def hue_sat_lum(rgb):
    mx, mn = rgb.max(axis=2), rgb.min(axis=2)
    sat = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0.0)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    d = np.maximum(mx - mn, 1e-6)
    hue = np.where(mx == r, ((g - b) / d) % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) * 60
    lum = 0.30 * r + 0.59 * g + 0.11 * b
    return hue, sat, lum


def smooth(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def recolor(img, box=None, protect=(), accent_boxes=()):
    """Wood -> steel, glossy blue -> gunmetal, white fields -> light steel, yellow (the
    selected state) -> the orange accent. Weights follow hue and saturation smoothly, so
    anti-aliased edges blend. `protect`: boxes (atlas coordinates) left as they are."""
    original = img
    region = img.crop(box) if box else img
    a = np.asarray(region).astype(float) / 255
    rgb, alpha = a[..., :3], a[..., 3:]
    hue, sat, lum = hue_sat_lum(rgb)
    wood = smooth(sat, 0.15, 0.35) * ((hue > 10) & (hue < 45))
    gold = smooth(sat, 0.35, 0.55) * ((hue >= 45) & (hue < 65))
    in_accent = np.zeros(lum.shape, bool)
    ox, oy = box[:2] if box else (0, 0)
    for x0, y0, x1, y1 in accent_boxes:
        in_accent[max(0, y0 - oy):max(0, y1 - oy), max(0, x0 - ox):max(0, x1 - ox)] = True
    wood = np.maximum(wood, gold * ~in_accent)
    gold = gold * in_accent
    blue = smooth(sat, 0.20, 0.40) * ((hue > 190) & (hue < 265))
    white = (1 - smooth(sat, 0.05, 0.12)) * smooth(lum, 0.75, 0.9)
    steel = np.clip((lum - 0.45) * 1.2 + 0.47, 0, 1)[..., None] * STEEL
    gun = np.clip(lum * 1.45 + 0.04, 0, 1)[..., None] * GUNMETAL
    field = lum[..., None] * FIELD
    out = rgb
    out = out * (1 - wood[..., None]) + steel * wood[..., None]
    out = out * (1 - blue[..., None]) + gun * blue[..., None]
    out = out * (1 - white[..., None]) + field * white[..., None]
    accent = np.clip(lum[..., None] * ACCENT, 0, 1)
    out = out * (1 - gold[..., None]) + accent * gold[..., None]
    res = Image.fromarray((np.dstack([np.clip(out, 0, 1), alpha]) * 255 + 0.5).astype(np.uint8), "RGBA")
    if box:
        img = img.copy()
        img.paste(res, box[:2])
    else:
        img = res
    for p in protect:
        img.paste(original.crop(p), p[:2])
    return img


# ---- steel frame pieces for wrapper_pieces_01 ------------------------------------------------

BAR = 25    # the visible bar, at the inner side of the game's 42 px border band
BAND = 42


def bar_tile(height):
    """A seamless horizontal piece of the riveted steel bar, `height` px tall."""
    t = horizontal_tile(element(5), 0, 30)
    return t.resize((round(t.width * height / t.height), height), Image.LANCZOS)


def bar(length, height, vertical=False):
    t = bar_tile(height)
    out = Image.new("RGBA", (length, height))
    for x in range(0, length, t.width):
        out.alpha_composite(t, (x, 0))
    return out.rotate(90, expand=True) if vertical else out


def corner_plate(size):
    """The square plate where two bars meet, with a screw (sheet_01 element 18)."""
    plate = element(5).crop((6, 6, 74, 74)).resize((size, size), Image.LANCZOS)
    screw = element(18)
    s = round(size * 0.55)
    plate.alpha_composite(screw.resize((s, s), Image.LANCZOS), ((size - s) // 2, (size - s) // 2))
    return plate


def with_shadow(img, off=3, blur=4, strength=0.45):
    a = np.asarray(img)[..., 3].astype(float) * strength
    sh = Image.fromarray(a.astype(np.uint8)).filter(ImageFilter.GaussianBlur(blur))
    out = Image.new("RGBA", img.size)
    shadow = Image.new("RGBA", img.size, (0, 0, 0, 255))
    shadow.putalpha(sh)
    out.alpha_composite(shadow, (off, off))
    out.alpha_composite(img)
    return out


def frame_square(size=294):
    """The square frame whose cells are the IFACEBORDER corners, edges and extras."""
    sq = Image.new("RGBA", (size, size))
    o = BAND - BAR  # outer margin of the band
    sq.alpha_composite(bar(size - 2 * o, BAR), (o, o))
    sq.alpha_composite(bar(size - 2 * o, BAR), (o, size - BAND))
    sq.alpha_composite(bar(size - 2 * o, BAR, True), (o, o))
    sq.alpha_composite(bar(size - 2 * o, BAR, True), (size - BAND, o))
    plate = corner_plate(BAR)
    for x, y in ((o, o), (size - BAND, o), (size - BAND, size - BAND), (o, size - BAND)):
        sq.alpha_composite(plate, (x, y))
    return with_shadow(sq)


def edge_strip(length, side):
    """An edge strip: `side` top / bottom / left / right, bar at the band's inner side."""
    o = BAND - BAR
    if side in ("top", "bottom"):
        s = Image.new("RGBA", (length, BAND))
        s.alpha_composite(bar(length, BAR), (0, o if side == "top" else 0))
    else:
        s = Image.new("RGBA", (BAND - 1, length))
        s.alpha_composite(bar(length, BAR, True), (o - 1 if side == "left" else 0, 0))
    return with_shadow(s)


def build_wrapper_01(game):
    src = read_tga(os.path.join(game, "art", "wrapper_pieces_01.tga"))
    out = recolor(src)
    clear = Image.new("RGBA", (1, 1))

    def put(img, x, y, w, h):
        out.paste(clear.resize((w, h)), (x, y))
        out.alpha_composite(img.crop((0, 0, w, h)), (x, y))

    put(frame_square(), 1, 1, 294, 294)
    put(edge_strip(295, "top"), 1, 338, 295, BAND)
    put(edge_strip(295, "bottom"), 1, 295, 295, BAND)
    put(edge_strip(262, "right"), 295, 1, 41, 262)
    put(edge_strip(262, "left"), 339, 1, 41, 262)
    # Section bars (small_bar under a frame's top edge, large_bar between sections).
    put(with_shadow(bar(205, 9), 1, 1), 45, 44, 205, 11)
    put(with_shadow(bar(190, 11), 1, 1), 295, 264, 190, 13)
    return out


def main(game, out):
    os.makedirs(PREVIEW, exist_ok=True)
    w1 = build_wrapper_01(game)
    w2 = recolor(read_tga(os.path.join(game, "art", "wrapper_pieces_02.tga")), protect=PROTECT_02,
                 accent_boxes=ACCENT_02)
    gs = recolor(read_tga(os.path.join(game, "art", "Wrapper_pieces_gamespy.tga")), GAMESPY_CHROME,
                 PROTECT_GAMESPY)
    for name, img in (("wrapper_pieces_01.tga", w1), ("wrapper_pieces_02.tga", w2),
                      ("Wrapper_pieces_gamespy.tga", gs)):
        write_tga(img, os.path.join(out, "art", name))
        shown = Image.new("RGBA", img.size, (255, 0, 255, 255))
        shown.alpha_composite(img)
        shown.convert("RGB").save(os.path.join(PREVIEW, name.lower().replace(".tga", ".png")))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
