r"""Builds the "Revived" main menu theme into mod_override\ (read by the DLL's file overrides).

    python build_menu_theme.py <game folder> <out mod_override folder>

Inputs: the elements cut by cut_sprites.py (ui-theme\elements\sheet_01) and the game's own
files (logo maps, menu overlay, Data\topmenu.xml), which are only read.

Outputs (paths relative to the game folder, under <out>):
  art\revived\menu_parts.tga   frame corners/edges, title plate, buttons (3 states)
  art\ron_logo_texture_map.tga logo: globe, gear and "Extended Edition" removed, "REVIVED" added
  art\ron_logo_normal_map.tga  matching normal map (the logo is lit by the game)
  art\anim\anim_overlay.tga    menu vignette with the HUD corner brackets on top
  art\anim\anim_bg_0_layer_N.tga  destroyer scene layers redrawn at 2560x1440, one per
                               source\ship_scene\hd\layer_N_*.jpg|png present ("magenta" in the
                               name: drawn on magenta and keyed out like cut_sprites.py)
  art\anim\anim_bg_3_layer_N.tga  jeep scene layers redrawn (background, explosion, jeep), from
                               source\jeep_scene\hd (make_jeep_layers.py); the menu then plays
                               the destroyer and the jeep scenes in turn
  Data\topmenu.xml             TOPMENUSELECTION pointed at menu_parts.tga
  Data\interface.xml, optionswin.xml, setupwin.xml, workshopwin.xml
                               dialog buttons (OK / Cancel...) pointed at menu_parts.tga
  art\revived\nation_flags.tga nation flags for the setup screen (build_flags.py), declared
                               in setupwin.xml, with a wider nation column
  art\revived\qol_buttons.tga  round buttons of the in-game qol row (build_qol_buttons.py),
                               declared in interface.xml (IFACEMAINBASE); the in-game menu
                               (MAINMENU) and the end of game window (ENDGAMEWIN) get the
                               steel dialog buttons too
                               and the resource panel is 25% larger, in the worker-bars
                               layout, with the population and city counters on its first
                               row (hud_layout)
  art\revived\rank_icons.tga   veteran rank insignia (build_rank_icons.py), declared next to
                               the qol buttons
  art\revived\minimap_frame.tga the steel frame round the folded minimap (build_minimap_frame.py),
                               declared there too
  art\revived\assist_panel.tga the AI assistant panel (build_assist_panel.py), declared there too
  art\revived\unit_bans.tga    the game setup's Units window (build_unit_bans.py), in setupwin.xml
  art\revived\hud_digits.tga   the resource panel's digits from a real font (build_hud_digits.py),
                               declared there too
"""
import os
import re
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont

import build_assist_panel
import build_unit_bans
import build_flags
import build_hud_digits
import build_qol_buttons
import build_survival_bar
import build_survival_cards
import build_rank_icons
import build_minimap_frame

HERE = os.path.dirname(os.path.abspath(__file__))
ELEMENTS = os.path.join(HERE, "..", "elements", "sheet_01")
SHIP_HD = os.path.join(HERE, "..", "source", "ship_scene", "hd")
JEEP_HD = os.path.join(HERE, "..", "source", "jeep_scene", "hd")
# The menu draws an animation layer at a size proportional to its IFACETEX rect (made for
# 1280x720), so a larger texture shows magnified, not sharper: keep the game's size.
SCENE_SIZE = (1280, 720)
# Per layer: (scale, dx, dy) applied after resizing, as fractions of the scene; scaling is
# anchored at the bottom-left corner. Smaller sea and spray, so the ship stays visible.
LAYER_ADJUST = {2: (1.0, 0.0, 0.07), 3: (1.0, 0.0, 0.07), 4: (0.78, 0.0, 0.03)}


def element(n):
    return Image.open(os.path.join(ELEMENTS, f"element_{n:02d}.png")).convert("RGBA")


def write_tga(img, path):
    """Uncompressed 32-bit, bottom-up: the layout of the game's own interface textures."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    w, h = img.size
    a = np.asarray(img.convert("RGBA"))[::-1]  # bottom-up
    bgra = a[..., [2, 1, 0, 3]].tobytes()
    with open(path, "wb") as f:
        f.write(bytes([0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0]) + struct.pack("<HHBB", w, h, 32, 8) + bgra)
    print(f"{path}: {w}x{h}")


def read_tga(path):
    return Image.open(path).convert("RGBA")


# ---- menu_parts.tga ------------------------------------------------------------------------

CORNER = 80        # corner pieces, square
EDGE_T = 30        # thickness of the edge strips (the bar is ~22 px, at the outer side)
TILE_V = 40        # length of the vertical edge tiles
# Button pieces: the game draws them at their texture size (TopMenuSelection::on_button_redraw,
# end pieces with add_smooth, the middle tiled), so the texture height is the button height.
# BTN_H rows hold a BTN_PLATE_H plate with transparent rows above and below: the gap between
# stacked buttons. The three states sit BTN_H + 1 apart, in rows ATLAS_H added under the old
# 512 (2026-11-10: taller buttons, the user: "un pic mai inalte si mai putin late", so the
# shorter titles sit well in them; they no longer fit above the title plate at 300..374).
ATLAS_H = 1024
BTN_X, BTN_Y, BTN_H = 1, 600, 56
BTN_PLATE_H = 46  # 10 px between stacked buttons (the rows are BTN_H apart)
BTN_L, BTN_M, BTN_R = 19, 291, 19
# Menu buttons and title are MENU_W wide instead of the game's 454, and the menu windows
# shrink by the same amount (548 -> 434), so the frame around them, which follows the
# window, is as narrow as the buttons. The plate is drawn 320 px wide, centred: the end
# pieces carry BTN_PAD transparent pixels.
MENU_W = 270  # 340 before 2026-11-10 (the titles are short now), then 10 % less
MENU_BUTTON = (MENU_W, BTN_H)  # layout size of the menu buttons (the game's: 454x35)
MENU_WINDOW_W = 548 - (454 - MENU_W)
BTN_PAD = 10  # the middle piece is tiled to the button's width, so it does not change with it
BTN_L_TEX, BTN_R_TEX = BTN_L + BTN_PAD, BTN_R + BTN_PAD  # widths of the padded end pieces
# Dialog buttons ("large_button" in interface.xml and the setup / options / workshop screens),
# same piece sizes as the game's, in the free rows 150..241 of the atlas.
POP_X, POP_Y, POP_H = 1, 150, 30
POP_L, POP_M, POP_R = 17, 142, 17
POP_FILES = ("interface.xml", "optionswin.xml", "setupwin.xml", "workshopwin.xml")
TITLE_Y, TITLE_H, TITLE_CAP, TITLE_MID = 300, 74, 48, 32
SMALL_CORNER, SMALL_EDGE = 40, 15  # the half-size frame for message boxes


def stripe_period(strip):
    """Horizontal repeat of a patterned bar (autocorrelation of its mean row)."""
    row = np.asarray(strip.convert("L"), dtype=float).mean(axis=0)
    row -= row.mean()
    best, period = -1e9, 8
    for p in range(4, 24):
        c = np.dot(row[:-p], row[p:]) / (len(row) - p)
        if c > best:
            best, period = c, p
    return period


def horizontal_tile(frame, y0, y1, margin=90, length=48):
    """A seamless piece of the top or bottom bar: a whole number of stripe periods."""
    w = frame.width
    band = frame.crop((margin, y0, w - margin, y1))
    inset = 4 if band.height >= 20 else 2
    p = stripe_period(band.crop((0, inset, band.width, band.height - inset)))
    n = max(1, length // p)
    start = (band.width - n * p) // 2
    return band.crop((start, 0, start + n * p, band.height))


def plate_states(plate, h, l, m, r):
    """Normal / hover / pressed buttons from the dark metal plate, 3-slice layout: end
    pieces l and r wide, middle m wide, all h tall."""
    w = round(plate.width * h / plate.height)
    p = plate.resize((w, h), Image.LANCZOS)
    left, right = p.crop((0, 0, l, h)), p.crop((w - r, 0, w, h))
    mid = p.crop((l + 6, 0, w - r - 6, h)).resize((m, h), Image.LANCZOS)
    normal = Image.new("RGBA", (l + m + r, h))
    normal.alpha_composite(left, (0, 0))
    normal.alpha_composite(mid, (l, 0))
    normal.alpha_composite(right, (l + m, 0))

    hover = ImageEnhance.Brightness(normal).enhance(1.25)
    glow = Image.new("RGBA", normal.size)
    d = ImageDraw.Draw(glow)
    wtot = normal.width
    d.rounded_rectangle([1, 1, wtot - 2, h - 2], radius=4, outline=(255, 150, 40, 255), width=2)
    glow = glow.filter(ImageFilter.GaussianBlur(1.6))
    edge = Image.new("RGBA", normal.size)
    d = ImageDraw.Draw(edge)
    d.rounded_rectangle([1, 1, wtot - 2, h - 2], radius=4, outline=(255, 185, 80, 230), width=1)
    # Bracket corners, like the concept's selection frame.
    for x0, sx in ((5, 1), (l - 1 + m + r - 6, -1)):
        for y0, sy in ((5, 1), (h - 6, -1)):
            d.line([(x0, y0), (x0 + 7 * sx, y0)], fill=(255, 200, 110, 255), width=1)
            d.line([(x0, y0), (x0, y0 + 6 * sy)], fill=(255, 200, 110, 255), width=1)
    hover.alpha_composite(glow)
    hover.alpha_composite(edge)
    # Glow only where the button is, so the transparent corners stay transparent.
    hover.putalpha(Image.fromarray(np.maximum(np.asarray(normal)[..., 3], np.asarray(glow)[..., 3] // 2)))

    pressed = ImageEnhance.Color(ImageEnhance.Brightness(normal).enhance(0.62)).enhance(0.6)
    return normal, hover, pressed


def button_states(plate):
    """The menu buttons: each state BTN_H tall with the BTN_PLATE_H plate centred."""

    def framed(img):
        out = Image.new("RGBA", (img.width, BTN_H))
        out.alpha_composite(img, (0, (BTN_H - BTN_PLATE_H) // 2))
        return out

    return tuple(framed(s) for s in plate_states(plate, BTN_PLATE_H, BTN_L, BTN_M, BTN_R))


def sliced(img, l, m):
    """A 3-slice row with a 1 px gap after each piece, as the game's atlases lay them out."""
    out = Image.new("RGBA", (img.width + 2, img.height))
    out.alpha_composite(img.crop((0, 0, l, img.height)), (0, 0))
    out.alpha_composite(img.crop((l, 0, l + m, img.height)), (l + 1, 0))
    out.alpha_composite(img.crop((l + m, 0, img.width, img.height)), (l + m + 2, 0))
    return out


def build_parts():
    atlas = Image.new("RGBA", (512, ATLAS_H))
    frame = element(5)  # 297x372 riveted metal frame
    fw, fh = frame.size
    corners = {
        "corner0": frame.crop((0, 0, CORNER, CORNER)),
        "corner1": frame.crop((fw - CORNER, 0, fw, CORNER)),
        "corner2": frame.crop((fw - CORNER, fh - CORNER, fw, fh)),
        "corner3": frame.crop((0, fh - CORNER, CORNER, fh)),
    }
    rects = {}
    for i, (name, img) in enumerate(corners.items()):
        x = i * CORNER
        atlas.alpha_composite(img, (x, 0))
        rects[name] = (x, 0, CORNER, CORNER)

    top = horizontal_tile(frame, 0, EDGE_T)
    bottom = horizontal_tile(frame, fh - EDGE_T, fh)
    left = frame.crop((0, 85, EDGE_T, 85 + TILE_V))           # the twin rods, no side plate
    right = frame.crop((fw - EDGE_T, 85, fw, 85 + TILE_V))
    y = CORNER + 4
    atlas.alpha_composite(top, (0, y)); rects["edge0"] = (0, y, top.width, EDGE_T)
    atlas.alpha_composite(bottom, (0, y + EDGE_T + 2)); rects["edge2"] = (0, y + EDGE_T + 2, bottom.width, EDGE_T)
    x = 340
    atlas.alpha_composite(right, (x, 0)); rects["edge1"] = (x, 0, EDGE_T, TILE_V)
    atlas.alpha_composite(left, (x + EDGE_T + 2, 0)); rects["edge3"] = (x + EDGE_T + 2, 0, EDGE_T, TILE_V)

    # The same frame at half size for small dialogs (the 450x150 message box): corners
    # SMALL_CORNER, edges SMALL_EDGE thick, in the free rows 84..124 right of the edge tiles.
    half = frame.resize((fw // 2, fh // 2), Image.LANCZOS)
    hw, hh = half.size
    c = SMALL_CORNER
    small = [half.crop((0, 0, c, c)), half.crop((hw - c, 0, hw, c)), half.crop((hw - c, hh - c, hw, hh)),
             half.crop((0, hh - c, c, hh))]
    for i, img in enumerate(small):
        sx = 60 + i * (c + 1)
        atlas.alpha_composite(img, (sx, 84))
        rects[f"small_corner{i}"] = (sx, 84, c, c)
    e = SMALL_EDGE
    sx = 60 + 4 * (c + 1) + 4
    top_s = horizontal_tile(half, 0, e, margin=45, length=24)
    bottom_s = horizontal_tile(half, hh - e, hh, margin=45, length=24)
    atlas.alpha_composite(top_s, (sx, 84)); rects["small_edge0"] = (sx, 84, top_s.width, e)
    atlas.alpha_composite(bottom_s, (sx, 84 + e + 2)); rects["small_edge2"] = (sx, 84 + e + 2, bottom_s.width, e)
    sx += 30
    side_r = half.crop((hw - e, 42, hw, 42 + TILE_V // 2))
    side_l = half.crop((0, 42, e, 42 + TILE_V // 2))
    atlas.alpha_composite(side_r, (sx, 84)); rects["small_edge1"] = (sx, 84, e, TILE_V // 2)
    atlas.alpha_composite(side_l, (sx + e + 2, 84)); rects["small_edge3"] = (sx + e + 2, 84, e, TILE_V // 2)

    # Title plate (the long riveted plate), 3-slice, plate vertically where the text sits.
    plate = element(2)
    ph = 56
    pw = round(plate.width * ph / plate.height)
    plate = plate.resize((pw, ph), Image.LANCZOS)
    title = Image.new("RGBA", (TITLE_CAP * 2 + TITLE_MID, TITLE_H))
    title.alpha_composite(plate.crop((0, 0, TITLE_CAP, ph)), (0, 2))
    # The middle is repeated across the title: the plate's average column (its vertical
    # shading) plus a little of its grain, so the repeats do not show.
    body = np.asarray(plate.crop((TITLE_CAP, 0, pw - TITLE_CAP, ph))).astype(float)
    column = body.mean(axis=1, keepdims=True)
    grain = body[:, : TITLE_MID] - body[:, : TITLE_MID].mean(axis=1, keepdims=True)
    mid = np.clip(column + 0.35 * grain, 0, 255)
    mid[..., 3] = column[..., 3]
    title.alpha_composite(Image.fromarray(mid.astype(np.uint8), "RGBA"), (TITLE_CAP, 2))
    title.alpha_composite(plate.crop((pw - TITLE_CAP, 0, pw, ph)), (TITLE_CAP + TITLE_MID, 2))
    atlas.alpha_composite(title, (0, TITLE_Y))
    rects["ribbon"] = [(0, TITLE_Y, TITLE_CAP, TITLE_H), (TITLE_CAP, TITLE_Y, TITLE_MID, TITLE_H),
                       (TITLE_CAP + TITLE_MID, TITLE_Y, TITLE_CAP, TITLE_H)]

    normal, hover, pressed = button_states(element(11))
    for img, yy in ((hover, BTN_Y - BTN_H - 1), (normal, BTN_Y), (pressed, BTN_Y + BTN_H + 1)):
        atlas.alpha_composite(img, (BTN_X, yy))
    # Dialog buttons (OK / Cancel...), laid out like the game's wrapper_pieces_02 ones:
    # highlighted, normal, disabled, POP_H + 1 apart.
    normal, hover, pressed = plate_states(element(11), POP_H, POP_L, POP_M, POP_R)
    for i, img in enumerate((hover, normal, pressed)):
        atlas.alpha_composite(sliced(img, POP_L, POP_M), (POP_X, POP_Y + i * (POP_H + 1)))
    return atlas, rects


# ---- logo ----------------------------------------------------------------------------------

def find_font(size):
    for name in ("bahnschrift.ttf", "segoeuib.ttf", "arialbd.ttf"):
        try:
            return ImageFont.truetype(os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts", name), size)
        except OSError:
            continue
    return ImageFont.load_default()


RIBBON_FROM = 318        # the ribbon's arched top edge is searched for below this row
LOGO_CUT = 336           # the white letters (the N's tail) reach down to here
GEAR_BOTTOM = 350        # lowest row of the gear; below it only the ribbon's tips
REVIVED_BOX = (275, 332, 404)  # centre x, top and bottom rows (the menu shows rows up to ~405)


def build_brand_logo(game):
    """The game's own logo (gold globe, gear, "RISE OF NATIONS", lit by the menu from its
    normal map, so it follows the cursor) with the "Extended Edition" ribbon replaced by
    REVIVED (brand.py), which gets a normal map from its own relief so it is lit too."""
    import brand
    from scipy import ndimage

    diffuse = read_tga(os.path.join(game, "art", "ron_logo_texture_map.tga"))
    normal = read_tga(os.path.join(game, "art", "ron_logo_normal_map.tga"))
    d = np.asarray(diffuse).astype(float) / 255
    rgb, a = d[..., :3], d[..., 3]
    mx, mn = rgb.max(axis=2), rgb.min(axis=2)
    sat = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
    ys = np.arange(a.shape[0])[:, None]
    # The ribbon is light gold; the gear above it is dark bronze. Per column, the ribbon starts
    # at the first light gold pixel below RIBBON_FROM (minus its 2 px dark rim); the gear and
    # globe above that line stay whole.
    gold = (a > 0.8) & (rgb[..., 0] > 0.6) & (rgb[..., 1] > 0.4) & (rgb[..., 2] < 0.4) & (sat > 0.45)
    gold[:RIBBON_FROM] = False
    top = np.where(gold.any(axis=0), gold.argmax(axis=0), a.shape[0]) - 2
    top = np.minimum(ndimage.median_filter(top, size=7, mode="nearest"), GEAR_BOTTOM)  # no ribbon tips
    # White letters (the N's tail) reach over the ribbon: they and their outline stay.
    letters = (a > 0.5) & (mx > 0.55) & (sat < 0.35)
    letters = ndimage.binary_dilation(letters, iterations=2)
    keep = (ys < top[None, :]) | ((ys < LOGO_CUT) & letters)
    tex = diffuse.copy()
    tex.putalpha(Image.fromarray((np.where(keep, a, 0) * 255).astype(np.uint8)))
    nmap = normal.copy()
    nmap.putalpha(tex.getchannel("A"))

    _, revived = brand.parts()
    cx, top, bottom = REVIVED_BOX
    h = bottom - top
    revived = revived.resize((round(h * revived.width / revived.height), h), Image.LANCZOS)
    pos = (cx - revived.width // 2, top)
    tex.alpha_composite(revived, pos)
    # Normals from the lettering's relief: luminance as height, softened.
    r = np.asarray(revived).astype(float) / 255
    height = ndimage.gaussian_filter((0.3 * r[..., 0] + 0.59 * r[..., 1] + 0.11 * r[..., 2]) * r[..., 3], 1.2)
    gy, gx = np.gradient(height)
    n = np.dstack([-gx * 6, gy * 6, np.ones_like(height)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    rn = np.dstack([(n * 0.5 + 0.5) * 255, r[..., 3] * 255]).astype(np.uint8)
    nmap.alpha_composite(Image.fromarray(rn, "RGBA"), pos)
    return tex, nmap


def build_logo(game):
    import brand

    if brand.available():
        return build_brand_logo(game)
    diffuse = read_tga(os.path.join(game, "art", "ron_logo_texture_map.tga"))
    normal = read_tga(os.path.join(game, "art", "ron_logo_normal_map.tga"))
    d = np.asarray(diffuse).astype(float) / 255
    rgb, a = d[..., :3], d[..., 3]
    mx, mn = rgb.max(axis=2), rgb.min(axis=2)
    sat = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
    ys = np.arange(a.shape[0])[:, None]
    # "RISE OF NATIONS" lettering: the light, unsaturated pixels of the title row, plus their
    # dark outline; the golden globe and gear behind are saturated and go away.
    band = (ys >= 228) & (ys <= 338)
    letters = band & (a > 0.5) & (mx > 0.55) & (sat < 0.35)
    from scipy import ndimage
    near = ndimage.binary_dilation(letters, iterations=4)
    keep = band & near & (a > 0.05) & ((sat < 0.45) | (mx < 0.28))
    keep = ndimage.binary_closing(keep, iterations=1)
    # Leftovers of the globe's rim between the letters are thin pieces not attached to a
    # letter body: keep only the pieces that contain lettering.
    lab, n = ndimage.label(keep)
    if n:
        has_letter = ndimage.maximum(letters, lab, index=np.arange(1, n + 1))
        keep &= np.isin(lab, np.nonzero(has_letter)[0] + 1)
    # Thin arcs of the globe's rim still touch the tops of "OF N": above the letter line,
    # only strokes at least ~5 px thick survive (the tall N's spike does).
    top_rows = ys < 248
    opened = ndimage.binary_opening(keep, structure=np.ones((3, 3)), iterations=2)
    keep = np.where(top_rows, opened, keep)
    out_a = np.where(keep, a, 0.0)
    # Silver instead of the original gold-rimmed white: grey from the luminance, a touch cool.
    lum = (0.30 * rgb[..., 0] + 0.59 * rgb[..., 1] + 0.11 * rgb[..., 2])[..., None]
    rgb = np.clip(lum * np.array([0.96, 0.98, 1.02]), 0, 1)

    # "— REVIVED —" in steel, under the title.
    W, H = diffuse.size
    text = Image.new("RGBA", (W, H))
    font = find_font(44)
    word, track = "REVIVED", 14
    widths = [font.getbbox(c)[2] - font.getbbox(c)[0] for c in word]
    total = sum(widths) + track * (len(word) - 1)
    cx, top = 288, 344
    x = cx - total // 2
    mask = Image.new("L", (W, H))
    md = ImageDraw.Draw(mask)
    for c, cw in zip(word, widths):
        md.text((x - font.getbbox(c)[0], top), c, font=font, fill=255)
        x += cw + track
    for x0, x1 in ((cx - total // 2 - 120, cx - total // 2 - 22), (cx + total // 2 + 22, cx + total // 2 + 120)):
        for xx in range(x0, x1):
            t = (xx - x0) / (x1 - x0)
            fade = t if x0 < cx else 1 - t  # strong next to the word, fading outwards
            md.line([(xx, top + 24), (xx, top + 26)], fill=int(255 * min(1, fade * 1.3)))
    m = np.asarray(mask).astype(float) / 255
    yy = (np.arange(H)[:, None] - top) / 50.0
    steel = np.clip(0.95 - 0.45 * yy, 0.45, 0.95)  # light at the top, darker below
    outline = ndimage.grey_dilation(m, size=(5, 5))
    tr = np.dstack([steel * 0.95, steel * 0.97, steel])
    rgb_new = np.where(keep[..., None], rgb, 0.0)
    a_new = out_a.copy()
    # outline (dark) under the steel letters
    rgb_new = rgb_new * (1 - outline[..., None]) + 0.06 * outline[..., None]
    a_new = np.maximum(a_new, outline * 0.95)
    rgb_new = rgb_new * (1 - m[..., None]) + tr * m[..., None]
    a_new = np.maximum(a_new, m)
    logo = Image.fromarray((np.dstack([rgb_new, a_new]) * 255 + 0.5).astype(np.uint8), "RGBA")

    n = np.asarray(normal).astype(float)
    flat = np.array([128.0, 128.0, 255.0])
    new_text = (outline > 0.02) & ~keep
    n_rgb = np.where(new_text[..., None], flat, n[..., :3])
    n_out = np.dstack([n_rgb, a_new * 255])
    nmap = Image.fromarray(np.clip(n_out, 0, 255).astype(np.uint8), "RGBA")
    return logo, nmap


# ---- overlay with HUD corners --------------------------------------------------------------

def build_overlay(game):
    ov = read_tga(os.path.join(game, "art", "anim", "anim_overlay.tga"))
    W, H = ov.size
    sx, sy = W / 1024, H / 572  # the element sheet is a full screen at 1024x572
    for n, (x, y) in ((1, (11, 7)), (4, (761, 7)), (14, (11, 331)), (15, (820, 331))):
        e = element(n)
        e = e.resize((round(e.width * sx), round(e.height * sy)), Image.LANCZOS)
        ov.alpha_composite(e, (round(x * sx), round(y * sy)))
    return ov


# ---- background (destroyer scene) ---------------------------------------------------------

def build_ship_scene(game, out):
    """Writes the redrawn layers; returns {layer index: (width, height)}."""
    from cut_sprites import key_out

    sizes = {}
    sea_tint = None
    if not os.path.isdir(SHIP_HD):
        return sizes
    for name in sorted(os.listdir(SHIP_HD)):
        m = re.match(r"layer_(\d)_.*\.(jpg|jpeg|png)$", name, re.I)
        if not m:
            continue
        img = Image.open(os.path.join(SHIP_HD, name)).convert("RGB")
        # Centre crop to 16:9, then scale to the scene size.
        w, h = img.size
        cw, ch = min(w, round(h * 16 / 9)), min(h, round(w * 9 / 16))
        img = img.crop(((w - cw) // 2, (h - ch) // 2, (w - cw) // 2 + cw, (h - ch) // 2 + ch))
        if "magenta" in name.lower():
            rgb = np.asarray(img, dtype=np.float64) / 255.0
            color, alpha = key_out(rgb)
            if int(m.group(1)) >= 2:
                # Sea and spray: the generator mixes magenta into the water itself, which the
                # key leaves violet (red == green). They are monochrome blue-grey anyway: keep
                # the brightness, take the colour of the clean water in the same image.
                clean = (alpha >= 0.95) & (np.minimum(rgb[..., 0], rgb[..., 2]) - rgb[..., 1] < 0.0)
                tint = np.median(color[clean], axis=0)
                tint /= tint @ np.array([0.299, 0.587, 0.114])
                # One colour for the water: the choppy waves take the base sea's (they came
                # out a more saturated, violet-looking blue).
                if int(m.group(1)) == 2:
                    sea_tint = tint
                elif int(m.group(1)) == 3 and sea_tint is not None:
                    tint = sea_tint
                lum = color @ np.array([0.299, 0.587, 0.114])
                color = np.clip(lum[..., None] * tint, 0, 1)
            rgba = np.dstack([color, alpha])
            img = Image.fromarray((rgba * 255 + 0.5).astype(np.uint8), "RGBA")
        img = img.convert("RGBA").resize(SCENE_SIZE, Image.LANCZOS)
        scale, dx, dy = LAYER_ADJUST.get(int(m.group(1)), (1.0, 0.0, 0.0))
        if (scale, dx, dy) != (1.0, 0.0, 0.0):
            W, H = SCENE_SIZE
            small = img.resize((round(W * scale), round(H * scale)), Image.LANCZOS)
            moved = Image.new("RGBA", SCENE_SIZE)
            moved.alpha_composite(small, (round(dx * W), H - small.height + round(dy * H)))
            img = moved
        n = int(m.group(1))
        write_tga(img, os.path.join(out, "art", "anim", f"anim_bg_0_layer_{n}.tga"))
        img.save(os.path.join(HERE, "..", f"preview_scene_layer_{n}.png"))
        sizes[n] = img.size
    # Layers not redrawn yet keep the game's art, recoloured to the new sky: their brightness
    # with the sky's colour, so the sepia sea does not clash with a blue-grey storm.
    sky = os.path.join(out, "art", "anim", "anim_bg_0_layer_0.tga")
    if 0 in sizes:
        lower = np.asarray(read_tga(sky).convert("RGB"), dtype=np.float64)[sizes[0][1] // 2:]
        tint = lower.reshape(-1, 3).mean(0)
        tint /= tint.mean()
        for n in range(1, 5):
            if n in sizes:
                continue
            src = read_tga(os.path.join(game, "art", "anim", f"anim_bg_0_layer_{n}.tga")).convert("RGBA")
            a = np.asarray(src, dtype=np.float64)
            lum = a[..., :3] @ np.array([0.299, 0.587, 0.114])
            rgb = np.clip(lum[..., None] * tint, 0, 255)
            img = Image.fromarray(np.dstack([rgb, a[..., 3]]).astype(np.uint8), "RGBA")
            write_tga(img, os.path.join(out, "art", "anim", f"anim_bg_0_layer_{n}.tga"))
            img.save(os.path.join(HERE, "..", f"preview_scene_layer_{n}.png"))
    return sizes


def build_jeep_scene(out):
    """The jeep scene (anim_bg_3), from the layers make_jeep_layers.py cut out of the redrawn
    picture (background, explosion, jeep). Returns True when it was built."""
    if not os.path.isdir(JEEP_HD):
        return False
    names = sorted(n for n in os.listdir(JEEP_HD) if re.match(r"layer_\d_.*\.png$", n))
    assert [int(n[6]) for n in names] == [0, 1, 2], names
    for name in names:
        img = Image.open(os.path.join(JEEP_HD, name)).convert("RGBA")
        # Centre crop to 16:9 (the pictures are 1024x572), then scale to the scene size.
        w, h = img.size
        cw, ch = min(w, round(h * 16 / 9)), min(h, round(w * 9 / 16))
        img = img.crop(((w - cw) // 2, (h - ch) // 2, (w - cw) // 2 + cw, (h - ch) // 2 + ch))
        img = img.resize(SCENE_SIZE, Image.LANCZOS)
        n = int(name[6])
        write_tga(img, os.path.join(out, "art", "anim", f"anim_bg_3_layer_{n}.tga"))
        img.save(os.path.join(HERE, "..", f"preview_jeep_layer_{n}.png"))
    return True


# ---- topmenu.xml ---------------------------------------------------------------------------

SHORT_TITLES = {
    "Learn to Play": "Tutorial",
    "Conquer the World Campaigns": "Campaign",
    "Conquer the World Campaign": "Campaign",
    "Solo Game": "Solo",
    "Multiplayer Game": "Multiplayer",
    "Options &amp; Profiles": "Options",
    "Tools &amp; Extras": "Extras",
    "Load a Scenario": "Scenarios",
    "Load Saved Game": "Load Game",
    "Recorded Game Playback": "Replays",
    "Back to Main Menu": "Main Menu",
    "Back to Multiplayer Menu": "Multiplayer",
    "QuickMatch": "Quick Match",
    "Game Browser": "Browse Games",
    "Scenario Editor": "Editor",
    "About Rise of Nations": "About",
    "View Credits": "Credits",
    "Map Balance Tool": "Map Balance",
    "Steam Workshop": "Workshop",
}


# String::generate_hash 0x00A1B6B0 (the hash string_lookup.xml keys its texts by, and hash_i of
# the lower-case text): over the characters from the last to the first, p = PRIMES[i % 50],
# hash += p * c + PRIMES[c % 50] * (i + 1), 32 bits. PRIMES = the exe's table at 0x00B14500.
PRIMES = [127, 811, 1597, 2131, 2749, 4759, 5527, 5953, 8117, 9539, 10273, 10753, 11159, 12301, 13217,
          14207, 15413, 17681, 18661, 19013, 21089, 22051, 25111, 25801, 27457, 28057, 29581, 30809,
          32611, 34469, 36067, 37511, 38723, 40093, 41983, 43321, 45083, 47431, 49667, 50767, 53453,
          55469, 57193, 59369, 61987, 65071, 73421, 77849, 84223, 89009]


def game_hash(text):
    h = hi = 0
    for i in range(len(text) - 1, -1, -1):
        c, lc, p = ord(text[i]), ord(text[i].lower()), PRIMES[i % 50]
        h = (h + p * c + PRIMES[c % 50] * (i + 1)) & 0xFFFFFFFF
        hi = (hi + p * lc + PRIMES[lc % 50] * (i + 1)) & 0xFFFFFFFF
    return h, hi


def build_string_lookup(game):
    r"""The game's Data\string_lookup.xml with the short menu titles it does not have (SHORT_TITLES)
    in one more CAT: the menus translate a button's title by the hash of its English text through
    this file, and the translations (translations\, string_lookup.xml.<id>) then give every
    language its own word. Checked: game_hash("Quick Battle") = 7943953 / 8581341 as in the file."""
    src = open(os.path.join(game, "Data", "string_lookup.xml"), encoding="utf-8").read()
    have = set(re.findall(r">([^<]*)</STRING>", src))
    new = []
    for title in dict.fromkeys(list(SHORT_TITLES.values()) + ["Play"]):
        text = title.replace("&amp;", "&")
        if text in have or title in have:
            continue
        h, hi = game_hash(text)
        new.append(f'\t\t<STRING hash="{h}" hash_i="{hi}" needed="1">{title}</STRING>')
    if not new:
        return src
    cat = '\t<CAT hash="%d">\n%s\n\t</CAT>\n' % (game_hash("revived_menu")[0], "\n".join(new))
    end = src.rindex("</ROOT>")
    return src[:end] + cat + src[end:]


def tex(name, rect):
    x, y, w, h = rect
    return f'<IFACETEX name="{name}" texture="TEX_REVIVED_MENU" left="{x}" top="{y}" width="{w}" height="{h}"/>'


def build_xml(game, rects, scene_sizes, jeep=False):
    src = open(os.path.join(game, "Data", "topmenu.xml"), encoding="utf-8").read()
    start = src.index("<TOPMENUSELECTION")
    end = src.index("</TOPMENUSELECTION>", start)
    block = src[start:end]
    lines = [
        '<IFACETEXLOAD id="TEX_REVIVED_MENU" file=".\\art\\revived\\menu_parts.tga" pixformat="PF_32"/>',
        f'<IFACERECT name="title" left="41" top="22" width="{MENU_W}" height="74"/>',
        f'<IFACERECT name="title_text" left="41" top="22" width="{MENU_W}" height="60"/>',
        tex("button_left", (BTN_X, BTN_Y, BTN_L_TEX, BTN_H)),
        tex("button_mid", (BTN_X + BTN_L_TEX + 1, BTN_Y, BTN_M, BTN_H)),
        tex("button_right", (BTN_X + BTN_L_TEX + 1 + BTN_M + 1, BTN_Y, BTN_R_TEX, BTN_H)),
    ]
    lines += [tex("ribbon", r) for r in rects["ribbon"]]
    lines.append('<IFACEBORDER name="border">')
    for k in ("corner0", "corner1", "corner2", "corner3", "edge0", "edge1", "edge2", "edge3"):
        lines.append("  " + tex(k, rects[k]))
    lines.append("</IFACEBORDER>")
    head = block[: block.index(">") + 1]
    new_block = head + "\n      " + "\n      ".join(lines) + "\n    "
    xml = src[:start] + new_block + src[end:]
    # Background: the redrawn scenes only, in turn (destroyer, then jeep once it is built); the
    # old ambush and horse scenes are dropped, their textures stay declared, unused.
    for scene in ("Ambush", "Horse") + (() if jeep else ("Jeep",)):
        xml, n = re.subn(r"[ \t]*<!-- %s animation -->\s*<ANIM\b.*?</ANIM>[ \t]*\r?\n" % scene, "", xml, flags=re.S)
        assert n == 1, scene
    # Menu buttons narrower and a little taller, and the menu windows (whose width the frame
    # follows) narrower by as much.
    xml, k = re.subn(r'(<BUTTON\b[^>]*?)width="454" height="35"', r'\g<1>width="%d" height="%d"' % MENU_BUTTON, xml)
    # The menus place their buttons inc_y apart (36 in the game's file), not by their height: with
    # BTN_H tall buttons that step made them overlap and the gap left in the texture was covered
    # (the user, 2026-11-10, twice). One button row a step, so the plates stand 10 px apart.
    xml, k = re.subn(r'(<POSITIONING\b[^>]*?)inc_y="36"', r'\g<1>inc_y="%d"' % BTN_H, xml)
    assert k >= 10, "menu button steps"
    assert k > 0, "menu buttons"
    xml, k = re.subn(r'(<[A-Z]+ name="[^"]*" title="[^"]*" left="30" top="40" )width="548" height="768"',
                     r'\g<1>width="%d" height="768"' % MENU_WINDOW_W, xml)
    assert k >= 5, "menu windows"
    # Short button titles (the user, 2026-11-10: "nu imi plac cuvintele astea lungi"). Titles the
    # game's string_lookup.xml has (Multiplayer, Options, Load Game, Main Menu, ...) stay translated
    # in every language; the others show in English there.
    for old, new in SHORT_TITLES.items():
        xml = re.sub(r'(<BUTTON name="[^"]*" title=")%s(")' % re.escape(old), r"\g<1>%s\g<2>" % new, xml)
    # The main menu: Play, Options, Extras, Exit (the user, 2026-11-10); Play opens the PLAY panel
    # with Tutorial, Campaign, Solo, Multiplayer and Main Menu (camera-smooth\src\survival.c opens it
    # for the "play_menu" button, as the game opens SOLO for "solo_menu").
    def play_main(m):
        block = m.group(0)
        names = ("tutorial", "ctw", "solo_menu", "solo", "mp_menu")
        rows = re.findall(r'[ \t]*<BUTTON name="([^"]*)"[^>]*/>\r?\n', block)
        first = None
        for row in re.finditer(r'([ \t]*)<BUTTON name="([^"]*)"[^>]*/>\r?\n', block):
            if row.group(2) in names and first is None:
                first = row
        if first is None:
            return block
        size = re.search(r'width="\d+" height="\d+"', first.group(0)).group(0)
        play = '%s<BUTTON name="play_menu" title="Play" %s/>\n' % (first.group(1), size)
        out, done = [], False
        for line in block.splitlines(keepends=True):
            mm = re.match(r'[ \t]*<BUTTON name="([^"]*)"', line)
            if mm and mm.group(1) in names:
                if not done:
                    out.append(play)
                    done = True
                continue
            out.append(line)
        return "".join(out)

    xml, k = re.subn(r"<MAINMENU\b.*?</MAINMENU>", play_main, xml, flags=re.S)
    assert k >= 2, "main menus"

    def play_panel(m):
        solo = m.group(0)
        head = solo[: solo.index(">") + 1].replace("<SOLO", "<PLAY").replace(
            'name="Solo Game Menu" title="Solo Game Menu"', 'name="Play" title="Play"')
        body = solo[solo.index(">") + 1: solo.rindex("</SOLO>")]
        rows = re.findall(r'([ \t]*)<BUTTON name="([^"]*)" title="[^"]*" (width="\d+" height="\d+")/>', body)
        ind, _, size = rows[0]
        buttons = "".join('%s<BUTTON name="%s" title="%s" %s/>\n' % (ind, n, t, size) for n, t in (
            ("tutorial", "Tutorial"), ("ctw", "Campaign"), ("solo_menu", "Solo"), ("mp_menu", "Multiplayer"),
            ("back", "Main Menu")))
        first = body.index(rows[0][0] + "<BUTTON")
        last = body.rindex("/>") + 2
        last = body.index("\n", last) + 1
        panel = head + body[:first] + buttons + body[last:] + "</PLAY>"
        return panel + "\n    " + solo

    xml, k = re.subn(r"<SOLO\b.*?</SOLO>", play_panel, xml, flags=re.S)
    assert k >= 2, "play panels"
    # The Solo menu: a Survival button under Quick Battle (camera-smooth\src\survival.c opens the
    # Quick Battle setup with the Survival game, the user, 2026-11-10). The menu's frame follows
    # its buttons, as the main menu's seven do. Only the SOLO blocks: MAINMENU has a solo button too.
    def add_survival(m):
        return re.sub(r'([ \t]*)(<BUTTON name="solo" title="Quick Battle" [^>]*/>)',
                      lambda b: b.group(1) + b.group(2) + "\n" + b.group(1) + b.group(2).replace(
                          'name="solo" title="Quick Battle"', 'name="survival" title="Survival"'), m.group(0))
    xml, k = re.subn(r"<SOLO\b.*?</SOLO>", add_survival, xml, flags=re.S)
    assert k >= 2 and xml.count('name="survival"') == k, "survival button"
    for n, (w, h) in scene_sizes.items():
        xml, k = re.subn(r'(<IFACETEX name="anim_bg0_tex%d"[^>]*?)width="\d+" height="\d+"' % n,
                         r'\g<1>width="%d" height="%d"' % (w, h), xml)
        assert k == 1, n
    return xml


def messagebox_frame(src, rects):
    """The message box ("Are you sure you want to quit?" and the game's other yes / no
    questions): the steel frame at half size instead of the wooden one, without the wooden
    ornaments (extra0..7) and the dark band behind the buttons."""
    start = src.index('<MESSAGEBOX name="messagebox"')
    end = src.index("</MESSAGEBOX>", start)
    box = src[start:end]
    for i in range(4):
        x, y, w, h = rects[f"small_corner{i}"]
        box, n = re.subn(r'<IFACETEX name="corner%d" texture="TEX_WRAPPER_0"[^>]*/>' % i,
                         tex(f"corner{i}", (x, y, w, h)), box)
        assert n == 1, ("messagebox corner", i)
        box, n = re.subn(r'<IFACETEX name="edge%d" texture="TEX_WRAPPER_0"[^>]*/>' % i,
                         tex(f"edge{i}", rects[f"small_edge{i}"]), box)
        assert n == 1, ("messagebox edge", i)
    box = re.sub(r'(<IFACETEX name="extra\d" texture=")TEX_WRAPPER_0(")', r"\1TEX_NONE\2", box)
    box, n = re.subn(r'(<IFACETEX name="button_border_(?:left|mid|right)" texture=")TEX_WRAPPER_0(")',
                     r"\1TEX_NONE\2", box)
    assert n == 3, "messagebox button band"
    return src[:start] + box + src[end:]


def popup_buttons(src):
    """The pop-up questions ("Are you sure you want to quit?" in the main menu is a POPUP,
    not the MESSAGEBOX): their buttons (butnormal / buthighlight / butdown 0..2, blue, from
    the diplomacy atlas) become the steel dialog buttons. Each POPUPSTYLE loads the atlas."""
    load = '<IFACETEXLOAD id="TEX_REVIVED_MENU" file=".\\art\\revived\\menu_parts.tga" pixformat="PF_32"/>'
    xs = ((POP_X, POP_L), (POP_X + POP_L + 1, POP_M), (POP_X + POP_L + 1 + POP_M + 1, POP_R))
    rows = {"buthighlight": POP_Y, "butnormal": POP_Y + POP_H + 1, "butdown": POP_Y + 2 * (POP_H + 1)}
    start = src.index("<POPUP>")
    end = src.index("</POPUP>", start)
    head, src, tail = src[:start], src[start:end], src[end:]
    out, n_styles = [], 0
    for part in re.split(r"(<POPUPSTYLE name=\"[^\"]*\">)", src):
        if part.startswith("<POPUPSTYLE"):
            part += "\n      " + load
            n_styles += 1
        out.append(part)
    src = "".join(out)
    for state, y in rows.items():
        for i, (x, w) in enumerate(xs):
            src, n = re.subn(r'<IFACETEX name="%s%d" texture="TEX_DIPLO"[^>]*/>' % (state, i),
                             tex(f"{state}{i}", (x, y, w, POP_H)), src)
            assert n == n_styles, (state, i, n)
    # The POPUPSTYLE loads are not seen in a game (the resign question drew white buttons:
    # texture index 0). The original buttons' TEX_DIPLO is loaded in IFACERENDERMANAGER, so the
    # atlas is declared there too.
    anchor = '<IFACETEXLOAD id="TEX_CONQ_RSRC" file="art\\CTW_Spec_Res.tga" pixformat="PF_32"/>'
    assert head.count(anchor) == 1, "global texture list"
    head = head.replace(anchor, anchor + "\n    " + load)
    return head + src + tail


# In-game windows with the pop-ups' blue buttons: the in-game menu (F10 / pause: Return to
# Game, Options, Save Game...) and the end of game window (Victory / Defeat: Examine Map,
# Game Stats, View Playback, Quit), plain and ranked.
GAME_BUTTON_WINDOWS = ("MAINMENU", "ENDGAMEWIN", "ENDGAMEWINRANKED")


def game_menu_buttons(src):
    """The blue buttons of GAME_BUTTON_WINDOWS become the steel ones too. The atlas comes
    from the global texture list (popup_buttons)."""
    xs = ((POP_X, POP_L), (POP_X + POP_L + 1, POP_M), (POP_X + POP_L + 1 + POP_M + 1, POP_R))
    rows = {"buthighlight": POP_Y, "butnormal": POP_Y + POP_H + 1, "butdown": POP_Y + 2 * (POP_H + 1)}
    for win in GAME_BUTTON_WINDOWS:
        start = src.index("<%s name=" % win)
        end = src.index("</%s>" % win, start)
        block = src[start:end]
        for state, y in rows.items():
            for i, (x, w) in enumerate(xs):
                block, n = re.subn(r'<IFACETEX name="%s%d" texture="TEX_DIPLO"[^>]*/>' % (state, i),
                                   tex(f"{state}{i}", (x, y, w, POP_H)), block)
                assert n == 1, (win, state, i)
        src = src[:start] + block + src[end:]
    return src


def capital_star(src):
    """The selection panel's own capital star goes behind the picture (the "object" button, 33,22
    60x35), out of sight: the picture is drawn after the panel and covered it. camera-smooth
    (panel.c) draws the star on the picture instead, at its top edge, in the middle."""
    old = '<IFACERECT name="capital_star" left="54" top="71" width="22" height="21"/>'
    assert src.count(old) == 1, "capital star"
    return src.replace(old, '<IFACERECT name="capital_star" left="52" top="26" width="22" height="21"/>')


def build_conquest_xml(game):
    """Data\\conquest.xml (Conquer the World's screens: the campaign choice, New / Load Campaign,
    Back, the battle windows) with the steel dialog buttons of menu_parts.tga (the user,
    2026-11-10): its "large_button_0/1/2" (wrapper_pieces_02 at 193 / 211 / 354, 47, the game's
    blue buttons) and the campaign choice's small "Continue" buttons ("small_button_0/1/2" on
    ctw_mapselect.tga). The atlas is loaded in each window block before its first use (texture
    ids are per window). The file has no language copies: its texts are looked up like the
    menus'."""
    src = open(os.path.join(game, "Data", "conquest.xml"), encoding="utf-8").read()
    load = '<IFACETEXLOAD id="TEX_REVIVED_MENU" file=".\\art\\revived\\menu_parts.tga" pixformat="PF_32"/>'
    xs = (POP_X, POP_X + POP_L + 1, POP_X + POP_L + 1 + POP_M + 1)
    ws = (POP_L, POP_M, POP_R)
    for part, old_x in enumerate((193, 211, 354)):
        pat = re.compile(r'<IFACETEX name="large_button_%d" texture="TEX_WRAPPER_1" left="%d" top="47" '
                         r'width="\d+" height="\d+"/>' % (part, old_x))
        new = (f'<IFACETEX name="large_button_{part}" texture="TEX_REVIVED_MENU" left="{xs[part]}" top="{POP_Y}" '
               f'width="{ws[part]}" height="{POP_H}"/>')
        text = (load + "\n    " + new) if part == 0 else new
        src, n = pat.subn(lambda m, t=text: t, src)  # a function: the path's backslashes stay as they are
        assert n, ("conquest large_button", part)
    for part, old_x in enumerate((314, 319, 432)):
        pat = re.compile(r'<IFACETEX name="small_button_%d" texture="TEX_CONQ_SETUPWIN1" left="%d" top="0" '
                         r'width="\d+" height="18"/>' % (part, old_x))
        new = (f'<IFACETEX name="small_button_{part}" texture="TEX_REVIVED_MENU" left="{xs[part]}" top="{POP_Y}" '
               f'width="{ws[part]}" height="{POP_H}"/>')
        src, n = pat.subn(lambda m, t=new: t, src)
        assert n == 1, ("conquest small_button", part, n)
    return src


def build_popup_xml(game, name, rects, flags):
    """`name` with its blue dialog buttons (wrapper_pieces_02 at 193,47) pointed at the
    steel ones in menu_parts.tga. The atlas is loaded right before each use, in the same
    window block, since texture ids are declared per window."""
    src = open(os.path.join(game, "Data", name), encoding="utf-8").read()
    load = '<IFACETEXLOAD id="TEX_REVIVED_MENU" file=".\\art\\revived\\menu_parts.tga" pixformat="PF_32"/>'
    pieces = (("left", 193, POP_X, POP_L), ("mid", 211, POP_X + POP_L + 1, POP_M),
              ("right", 354, POP_X + POP_L + 1 + POP_M + 1, POP_R))
    for part, old_x, x, w in pieces:
        old = f'<IFACETEX name="large_button_{part}" texture="TEX_WRAPPER_1" left="{old_x}" top="47"'
        new = f'<IFACETEX name="large_button_{part}" texture="TEX_REVIVED_MENU" left="{x}" top="{POP_Y}"'
        if part == "left":
            new = load + "\n    " + new
        n = src.count(old)
        assert n, (name, part)
        src = src.replace(old, new)
    if name == "interface.xml":
        src = messagebox_frame(src, rects)
        src = popup_buttons(src)
        src = game_menu_buttons(src)
        src = capital_star(src)
        src = hud_layout(src)
        src = qol_buttons(src)
    if name == "setupwin.xml":
        src = setup_margins(src)
        src = setup_flags(src, flags)
        src = setup_no_nukes(src)
        src = setup_hero_buttons(src)
    return src


# Game setup screen: the players and rules panels sat 2% from the screen edges (positions in
# tenths of a percent), on top of the theme's screen frame. 4% from the sides now, with the
# panels a little narrower so the gap between them stays the same, and 6.5% from the top:
# the frame's corner brackets reach down to 5% of the screen height.
SETUP_MARGINS = [
    ('<PLAYERSWINSOLO name="playerswin" left_percent="20" top_percent="26" percent_width="550"',
     '<PLAYERSWINSOLO name="playerswin" left_percent="40" top_percent="65" percent_width="520"'),
    ('<PLAYERSWINMP name="playerswin" left_percent="20" top_percent="26" percent_width="550"',
     '<PLAYERSWINMP name="playerswin" left_percent="40" top_percent="65" percent_width="520"'),
    ('<PLAYERSWINGAMESPY name="playerswin" left="20" top="85" percent_width="550"',
     '<PLAYERSWINGAMESPY name="playerswin" left_percent="40" top="85" percent_width="520"'),
    ('<PLAYERSWINRANKED name="playerswin" left="20" top="85" percent_width="550"',
     '<PLAYERSWINRANKED name="playerswin" left_percent="40" top="85" percent_width="520"'),
    ('<RULESWIN name="ruleswin" right_inverse_percent="20" top_percent="26" percent_width="-405"',
     '<RULESWIN name="ruleswin" right_inverse_percent="40" top_percent="65" percent_width="-395"'),
]

# Start / Back in a holder at the bottom right of the players panel (single player and LAN
# setup), twice the original width, at the original height. They stay in the panel:
# SetupWin finds "start" there to enable it, and a child window is clipped to its parent, so
# the holder ends at the panel's right edge. The panel spans 4..56% of the screen; the holder
# runs 492..1000 per mille of it (about a pixel each), 86 px tall (10% wider, 7% taller than
# the first try, 538..1000 x 80), too wide to be centred on the screen (centred, it could
# reach 769 at most). The oval's recess is rows 12..49 of 75 and starts 13 px in from the
# left, 18 px from the right: the buttons sit centred in it, taller, with a larger font.
SETUP_BUTTONS = [
    ('<BUTTON name="start" title="Start" left="54" bottom_inverse="40" percent_width="395" height="-30"/>',
     '<BUTTON name="start" title="Start" left_percent="535" bottom_inverse="47" percent_width="197" height="-38">\n'
     '        <FONT size="18" style="B"/>\n'
     '      </BUTTON>'),
    ('<BUTTON name="cancel" title="Back" right_inverse="59" bottom_inverse="40" percent_width="-395" height="-30"/>',
     '<BUTTON name="cancel" title="Back" right_inverse_percent="47" bottom_inverse="47" percent_width="-197" height="-38">\n'
     '        <FONT size="18" style="B"/>\n'
     '      </BUTTON>'),
    ('<IFACERECT left="30" right_inverse="30" bottom_inverse="15" height="-69"/>',
     '<IFACERECT left_percent="492" right_inverse="0" bottom_inverse="15" height="-86"/>'),
]
SETUP_BUTTON_WINDOWS = ("PLAYERSWINSOLO", "PLAYERSWINMP")


def setup_margins(src):
    for old, new in SETUP_MARGINS:
        assert src.count(old) == 1, old
        src = src.replace(old, new)
    for win in SETUP_BUTTON_WINDOWS:
        start = src.index("<%s " % win)
        end = src.index("</%s>" % win, start)
        block = src[start:end]
        for old, new in SETUP_BUTTONS:
            assert block.count(old) == 1, (win, old)
            block = block.replace(old, new)
        src = src[:start] + block + src[end:]
    return src


# Nation column wider (per mille of the players panel), taken from the player name column,
# to make room for the flag the DLL draws left of the nation name (camera-smooth\src\flags.c).
SETUP_COLUMNS = [
    ('<IFACERECT name="player" percent_width="367"', '<IFACERECT name="player" percent_width="322"', 1),
    ('<IFACERECT name="nation" percent_width="235"', '<IFACERECT name="nation" percent_width="280"', 1),
    ('<IFACERECT name="player" percent_width="337"', '<IFACERECT name="player" percent_width="292"', 3),
    ('<IFACERECT name="nation" percent_width="230"', '<IFACERECT name="nation" percent_width="275"', 3),
]


# Resource panel (top left) and the population / city counters 25% larger, the counters
# moved from the top right onto the panel's first row, right of the commerce cap, so the eye
# does not jump across the screen. The DLL draws their numbers larger by the same factor
# (camera-smooth\src\hud.c, hud_scale in ron_camera.ini).
HUD_SCALE = 1.25
# The worker-bars layout (camera-smooth\src\respanel.c): the workers column is gone, each
# resource row has a bar of its places of work and "used/places" at its end, so the resource
# column is wider (200 instead of 115; 158 until 2026-11-05 let a 5-digit amount, a 4-digit rate and "31|51" overlap); infra_button_rect1 is the size of the handle under the
# rows that shows / hides the bars; the market column opens right of the resources, so nothing
# is left of them any more. Sizes before HUD_SCALE.
RES_WIDTH = 200
PANEL_RECTS = {
    "main_rect": {"width": RES_WIDTH + 120},
    "resource_rect0": {"width": RES_WIDTH},
    "resource_rect1": {"width": RES_WIDTH},
    "resource_cap_rect": {"width": RES_WIDTH - 51},
    "resource_item_rect": {"width": RES_WIDTH - 8},
    "infra_button_rect1": {"width": 44, "height": 8},
}
RES_RIGHT = RES_WIDTH  # the end of the cap row; the DLL moves the counters with the panel's edge


def panel_rects(block):
    for name, attrs in PANEL_RECTS.items():
        def repl(m, name=name, attrs=attrs):
            tag = m.group(0)
            for key, value in attrs.items():
                tag, n = re.subn(r'\b%s="\s*-?\d+\s*"' % key, '%s="%d"' % (key, value), tag)
                assert n == 1, (name, key)
            return tag
        block, n = re.subn(r'<IFACERECT name="%s" [^>]*>' % name, repl, block)
        assert n == 1, name
    block, n = re.subn(r'(<IFACERESOURCES [^>]*\bwidth=")\d+', r"\g<1>%d" % (RES_WIDTH + 120), block, count=1)
    assert n == 1
    return block


def hud_layout(src):
    s = HUD_SCALE
    start = src.index("<IFACERESOURCES ")
    end = src.index("</IFACERESOURCES>", start)
    block = panel_rects(src[start:end])

    def scaled(m):
        return '%s="%d"' % (m.group(1), round(int(m.group(2)) * s))

    def scale_tag(m):
        return re.sub(r'\b(left|top|width|height|x|y)="\s*(-?\d+)\s*"', scaled, m.group(0))

    block, n = re.subn(r"<(?:IFACERECT|IFACEPOINT) [^>]*>", scale_tag, block)
    assert n == 20, n
    block = re.sub(r"<IFACERESOURCES [^>]*>", scale_tag, block, count=1)
    src = src[:start] + block + src[end:]

    left = round(RES_RIGHT * s) + 2
    h_area, h_text, w_pop, w_city = round(21 * s), round(17 * s), round(97 * s), round(67 * s)
    lines = {
        "cap_area": (left, 0, 5 + w_pop + 2 + w_city + 5, h_area),
        "pop_cap": (left + 5, 2, w_pop, h_text),
        "city_cap": (left + 5 + w_pop + 2, 2, w_city, h_text),
    }
    for name, (x, y, w, h) in lines.items():
        src, n = re.subn(r'<IFACERECT name="%s" right_inverse="\d+" top="\d+" width="-\d+" height="\d+"/>' % name,
                         '<IFACERECT name="%s" left="%d" top="%d" width="%d" height="%d"/>' % (name, x, y, w, h), src)
        assert n == 1, name
    return src


def qol_buttons(src):
    """The round buttons of the qol row (build_qol_buttons.py), in the IFACEMAINBASE block,
    where camera-smooth\\src\\panel.c looks them up with the interface's find_tex."""
    anchor = '<IFACETEXLOAD id="TEX_TIMERS" file="art\\iface_timers.tga"/>'
    assert src.count(anchor) == 1
    lines = ['<IFACETEXLOAD id="TEX_REVIVED_QOL" file=".\\%s" pixformat="PF_32"/>' % build_qol_buttons.ATLAS]
    lines += build_qol_buttons.tex_lines()
    lines += build_rank_icons.tex_lines()  # the rank insignia (ranks.c), looked up the same way
    lines += build_minimap_frame.tex_lines()  # the folded minimap's steel frame (minimap.c)
    lines += build_assist_panel.tex_lines()  # the AI assistant panel (assist_panel.c)
    lines += [build_hud_digits.load_line()] + build_hud_digits.tex_lines()  # the panel's digits (hud.c)
    lines += build_survival_bar.tex_lines()  # the Survival wave bar (survival.c)
    lines += build_survival_cards.tex_lines()  # the Survival reward cards (cards.c)
    # the command panel's empty cell, for the folded selection panel's background (selpanel.c)
    lines += ['<IFACETEX name="sel_blank" texture="TEX_CIV" left="211" top="20" width="60" height="38"/>']
    return src.replace(anchor, anchor + "\n    " + "\n    ".join(lines))


# The Nukes slider and the Units button (camera-smooth\src\nonukes.c and unitbans.c, drawn by the
# DLL over check boxes) and the setup screen's check boxes, the same in single player and
# multiplayer: the right column holds boxes 0-4 (Save to Profile ... Cooperative AI; the DLL shows
# Cooperative AI greyed in multiplayer, which has none), the left column's first row the slider
# with the Units button beside it, and the multiplayer-only boxes 5-10 (Allow Observer Chat ...
# Require Random Nations) the rest: 5-9 under the slider, 10 under Cooperative AI. In single
# player those stay hidden. The slider's art is the game's own: the horizontal track
# (interface.xml's slider_horz) and the caret of the options screen's sliders, both in
# TEX_WRAPPER_1.
LEFT_COLUMN = ('      <POSITIONING x="20" y="584" inc_y="26">\n' +
               "".join('        <CHECK name="check%d" show="0" percent_width="400" height="21"/>\n' % i
                       for i in range(5, 11)) +
               '      </POSITIONING>\n')
NO_NUKES = ('      <POSITIONING x="20" y="584">\n'
            '        <CHECK name="nonukes" show="0" percent_width="310" height="21"/>\n'
            '      </POSITIONING>\n'
            '      <CHECK name="unitbans" show="0" left_percent="360" top="584" percent_width="160" height="21"/>\n'
            '      <POSITIONING x="20" y="610" inc_y="26">\n' +
            "".join('        <CHECK name="check%d" show="0" percent_width="400" height="21"/>\n' % i for i in range(5, 10)) +
            '      </POSITIONING>\n'
            '      <POSITIONING y="714">\n'
            '        <CHECK name="check10" show="0" right_inverse="14" percent_width="-400" height="21"/>\n'
            '      </POSITIONING>\n')


NUKES_TEX_ANCHOR = '<IFACETEX name="check_on" texture="TEX_WRAPPER_1" left="1" top="70" width="25" height="22"/>'
NUKES_TEX = [
    '<IFACETEX name="nukes_track_l" texture="TEX_WRAPPER_1" left="113" top="380" width="5" height="16"/>',
    '<IFACETEX name="nukes_track_m" texture="TEX_WRAPPER_1" left="119" top="380" width="173" height="16"/>',
    '<IFACETEX name="nukes_track_r" texture="TEX_WRAPPER_1" left="293" top="380" width="5" height="16"/>',
    '<IFACETEX name="nukes_caret" texture="TEX_WRAPPER_1" left="382" top="0" width="17" height="28"/>',
]


def setup_no_nukes(src):
    assert src.count(LEFT_COLUMN) == 1
    src = src.replace(LEFT_COLUMN, NO_NUKES)
    assert src.count(NUKES_TEX_ANCHOR) == 1
    # the Units window's art, icon pages and frame (build_unit_bans.py, unitbans.c)
    return src.replace(NUKES_TEX_ANCHOR, NUKES_TEX_ANCHOR + "\n    " +
                       "\n    ".join(NUKES_TEX + build_unit_bans.tex_lines()))


# The Survival setup's hero choice (camera-smooth\src\survival.c, the user 2026-11-10): two
# buttons with the heroes' medallions in the solo players panel, at the bottom of the panel, under the
# computer rows of a co-op test (top_inverse: from the panel's bottom), hidden by default; survival.c shows them in the Survival setup only and draws them.
# Their pictures are declared in the SETUPWIN block too, where its find_tex looks.
HERO_BUTTONS = ('      <BUTTON name="hero_inf" title="" show="0" left="24" top_inverse="190" width="84" height="84"/>\n'
                '      <BUTTON name="hero_cav" title="" show="0" left="120" top_inverse="190" width="84" height="84"/>\n'
                # the game's goal (the user, 2026-11-11): what brings the final assault, a Wonder,
                # the Information Age or the 45 minutes; the game's own alert pictures
                '      <BUTTON name="goal_wonder" title="" show="0" left="252" top_inverse="190" width="84" height="84"/>\n'
                '      <BUTTON name="goal_tech" title="" show="0" left="348" top_inverse="190" width="84" height="84"/>\n'
                '      <BUTTON name="goal_time" title="" show="0" left="444" top_inverse="190" width="84" height="84"/>\n')
GOAL_TEX = ['<IFACETEXLOAD id="TEX_REVIVED_ALERTS" file="art\\iface_alerts.tga" pixformat="PF_32"/>',
            '<IFACETEX name="goal_wonder_icon" texture="TEX_REVIVED_ALERTS" left="0" top="149" width="63" height="45"/>',
            '<IFACETEX name="goal_tech_icon" texture="TEX_REVIVED_ALERTS" left="64" top="260" width="63" height="64"/>',
            '<IFACETEX name="goal_time_icon" texture="TEX_REVIVED_ALERTS" left="0" top="1" width="63" height="63"/>']


def setup_hero_buttons(src):
    head = '<PLAYERSWINSOLO name="playerswin"'
    i = src.index(head)
    j = src.index("\n", i) + 1
    src = src[:j] + HERO_BUTTONS + src[j:]
    anchor = '<IFACETEX name="team_color"'
    assert src.count(anchor) == 1
    lines = build_survival_bar.tex_lines() + GOAL_TEX
    return src.replace(anchor, "\n    ".join(lines) + "\n    " + anchor)


def setup_flags(src, flags):
    """The flag atlas and one IFACETEX "flag_<tribe index>" per flag, in the SETUPWIN block
    next to team_color, where the window's find_tex looks them up."""
    for old, new, n in SETUP_COLUMNS:
        assert src.count(old) == n, old
        src = src.replace(old, new)
    anchor = '<IFACETEX name="team_color"'
    assert src.count(anchor) == 1
    lines = ['<IFACETEXLOAD id="TEX_REVIVED_FLAGS" file=".\\%s" pixformat="PF_32"/>' % build_flags.ATLAS]
    lines += build_flags.tex_lines(flags)
    return src.replace(anchor, "\n    ".join(lines) + "\n    " + anchor)


def main(game, out):
    atlas, rects = build_parts()
    # The 3-slice buttons: each piece gets its own atlas slot with a 1 px gap (left, mid,
    # right), the end pieces with BTN_PAD transparent pixels on their outer side.
    fixed = Image.new("RGBA", atlas.size)
    fixed.alpha_composite(atlas)
    for yy in (BTN_Y - BTN_H - 1, BTN_Y, BTN_Y + BTN_H + 1):
        row = atlas.crop((BTN_X, yy, BTN_X + BTN_L + BTN_M + BTN_R, yy + BTN_H))
        fixed.paste((0, 0, 0, 0), (0, yy, 512, yy + BTN_H))
        fixed.alpha_composite(row.crop((0, 0, BTN_L, BTN_H)), (BTN_X + BTN_PAD, yy))
        fixed.alpha_composite(row.crop((BTN_L, 0, BTN_L + BTN_M, BTN_H)), (BTN_X + BTN_L_TEX + 1, yy))
        fixed.alpha_composite(row.crop((BTN_L + BTN_M, 0, BTN_L + BTN_M + BTN_R, BTN_H)),
                              (BTN_X + BTN_L_TEX + 1 + BTN_M + 1, yy))
    write_tga(fixed, os.path.join(out, "art", "revived", "menu_parts.tga"))
    fixed.save(os.path.join(HERE, "..", "preview_menu_parts.png"))

    logo, nmap = build_logo(game)
    write_tga(logo, os.path.join(out, "art", "ron_logo_texture_map.tga"))
    write_tga(nmap, os.path.join(out, "art", "ron_logo_normal_map.tga"))
    logo.save(os.path.join(HERE, "..", "preview_logo.png"))

    ov = build_overlay(game)
    write_tga(ov, os.path.join(out, "art", "anim", "anim_overlay.tga"))
    ov.save(os.path.join(HERE, "..", "preview_overlay.png"))

    scene_sizes = build_ship_scene(game, out)
    jeep = build_jeep_scene(out)
    xml = build_xml(game, rects, scene_sizes, jeep)
    path = os.path.join(out, "Data", "topmenu.xml")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, "w", encoding="utf-8").write(xml)
    print(path)
    path = os.path.join(out, "Data", "string_lookup.xml")
    open(path, "w", encoding="utf-8", newline="\r\n").write(build_string_lookup(game))  # CRLF, as the game's
    print(path)
    flags = build_flags.main(out)
    build_qol_buttons.main(out)
    build_rank_icons.main(out)
    build_minimap_frame.main(out)
    build_assist_panel.main(out)
    build_unit_bans.main(out, game)
    build_hud_digits.main(out)
    build_survival_bar.main(out)
    build_survival_cards.main(out, write_tga)
    path = os.path.join(out, "Data", "conquest.xml")
    open(path, "w", encoding="utf-8").write(build_conquest_xml(game))
    print(path)
    for name in POP_FILES:
        path = os.path.join(out, "Data", name)
        open(path, "w", encoding="utf-8").write(build_popup_xml(game, name, rects, flags))
        print(path)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
