r"""Repaints the game's shared interface atlases in the steel style, piece for piece in place,
so every window that uses them (game setup, options, dialogs...) changes without editing its
XML. Written to <out>\art\ under the same names; the game's files are only read.

    python build_reskin.py <game folder> <out mod_override folder>

  wrapper_pieces_01.tga        frames (IFACEBORDER pieces), section bars, the frame around
                               the Start / Back buttons: wood becomes steel
  wrapper_pieces_02.tga        buttons, tabs, check boxes, drop-down fields and arrows: glossy;
                               the drop-down fields grey with a grain (grain_fields)
  revived\setup_combo.tga      the same drop-down pieces 26 px high for the game setup screen
                               (setupwin.xml points combo_* there; tall_combo_atlas)
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


# ---- grainy grey drop-down fields in wrapper_pieces_02 ---------------------------------------

# combo_left / combo_mid / combo_right (interface.xml): x 193..196, 198..327, 329..331; the
# normal row at top 1, the disabled (_d) one at top 24, 21 px high. Inside each: a light rim,
# a dark line, the field (rows top+2 .. top+height-3), the dark line, the rim.
COMBO_H = 21
FIELD_GREY, FIELD_GREY_D = 182, 125   # field grey 0..255: normal, disabled (was 150 / 108)
COMBO_02 = dict(rows=[(1, FIELD_GREY), (24, FIELD_GREY_D)], height=COMBO_H,
                cols=[(195, 197), (198, 328), (329, 331)], mid=(198, 130),
                rims=[(193, 197), (198, 328), (329, 333)], sides=[193, 332])
GRAIN_SEED = 93

# The setup screen's taller pieces (revived\setup_combo.tga, 256x64): left 1..4, mid 6..135,
# right 137..140, arrow 142..169 (28 wide); normal row at top 1, disabled at top 29.
TALL_H = 26
TALL_ARROW_W = 28
TALL_X = {"left": 1, "mid": 6, "right": 137, "arrow": 142}
TALL_ROWS = (1, 29)
COMBO_TALL = dict(rows=[(1, FIELD_GREY), (29, FIELD_GREY_D)], height=TALL_H,
                  cols=[(3, 5), (6, 136), (137, 139)], mid=(6, 130),
                  rims=[(1, 5), (6, 136), (137, 141)], sides=[1, 140])


def grain(width, height, rng):
    """Film grain, seamless across `width` (the field's middle is tiled): fine per-pixel
    noise plus a softer, coarser layer. Mean 0, about +-0.05."""
    fine = rng.normal(0, 0.032, (height, width))
    coarse = rng.normal(0, 1, (height, width))
    coarse = (coarse + np.roll(coarse, 1, 1) + np.roll(coarse, -1, 1)
              + np.roll(coarse, 1, 0) + np.roll(coarse, -1, 0)) / 5 * 0.035
    return fine + coarse


def grain_fields(img, rows, height, cols, mid, rims, sides):
    """The white drop-down fields become a grey with a grain texture; the frame lines stay."""
    a = np.asarray(img).astype(float) / 255
    rng = np.random.default_rng(GRAIN_SEED)
    mid_x, mid_w = mid
    for top, grey in rows:
        y0, y1 = top + 2, top + height - 2
        noise = grain(mid_w, y1 - y0, rng)
        tint = grey / 255 * np.array([0.98, 0.99, 1.01])   # plain grey, barely cool
        for x0, x1 in cols:
            for x in range(x0, x1):
                n = noise[:, (x - mid_x) % mid_w]
                col = a[y0:y1, x]
                lum = col[:, :3] @ [0.30, 0.59, 0.11]
                inside = lum > 0.45                    # leave the dark frame lines
                depth = np.ones(y1 - y0)
                depth[:2] = (0.88, 0.95)               # a slight inner shadow under the top line
                new = np.clip(tint[None, :] * depth[:, None] + n[:, None], 0, 1)
                col[inside, :3] = new[inside]
        # The outer light rim (first and last row) is dimmed so the field does not read as
        # a white outline around a grey box.
        for y in (top, top + height - 1):
            for x0, x1 in rims:
                a[y, x0:x1, :3] *= 0.80
        a[top + 1:top + height - 1, sides, :3] *= 0.80
    return Image.fromarray((a * 255 + 0.5).astype(np.uint8), "RGBA")


def stretch_piece(piece, height):
    """A 21 px drop-down piece made `height` px high: the two frame rows at the top and at the
    bottom stay as they are, only the field between them is stretched."""
    w = piece.width
    out = Image.new("RGBA", (w, height))
    out.paste(piece.crop((0, 0, w, 2)), (0, 0))
    out.paste(piece.crop((0, 2, w, COMBO_H - 2)).resize((w, height - 4), Image.BILINEAR), (0, 2))
    out.paste(piece.crop((0, COMBO_H - 2, w, COMBO_H)), (0, height - 2))
    return out


def tall_combo_atlas(w2):
    r"""revived\setup_combo.tga from the recoloured (not yet grained) wrapper_pieces_02."""
    atlas = Image.new("RGBA", (256, 64))
    for top, src_top in zip(TALL_ROWS, (1, 24)):
        for name, x0, w in (("left", 193, 4), ("mid", 198, 130), ("right", 329, 4)):
            atlas.paste(stretch_piece(w2.crop((x0, src_top, x0 + w, src_top + COMBO_H)), TALL_H),
                        (TALL_X[name], top))
        arrow = w2.crop((123, src_top, 146, src_top + COMBO_H)).resize((TALL_ARROW_W, TALL_H), Image.LANCZOS)
        atlas.paste(arrow, (TALL_X["arrow"], top))
    return grain_fields(atlas, **COMBO_TALL)


def main(game, out):
    os.makedirs(PREVIEW, exist_ok=True)
    w1 = build_wrapper_01(game)
    w2 = recolor(read_tga(os.path.join(game, "art", "wrapper_pieces_02.tga")), protect=PROTECT_02,
                 accent_boxes=ACCENT_02)
    tall = tall_combo_atlas(w2)
    w2 = grain_fields(w2, **COMBO_02)
    gs = recolor(read_tga(os.path.join(game, "art", "Wrapper_pieces_gamespy.tga")), GAMESPY_CHROME,
                 PROTECT_GAMESPY)
    for name, img in (("wrapper_pieces_01.tga", w1), ("wrapper_pieces_02.tga", w2),
                      ("Wrapper_pieces_gamespy.tga", gs), (os.path.join("revived", "setup_combo.tga"), tall)):
        write_tga(img, os.path.join(out, "art", name))
        shown = Image.new("RGBA", img.size, (255, 0, 255, 255))
        shown.alpha_composite(img)
        shown.convert("RGB").save(os.path.join(PREVIEW, os.path.basename(name).lower().replace(".tga", ".png")))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
