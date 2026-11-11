r"""The art of the game setup's Units window (camera-smooth\src\unitbans.c), drawn here (no source
picture) in the theme's look, as the AI assistant panel (build_assist_panel.py): dark brushed
gunmetal, gold rims, round gold buttons.

    python build_unit_bans.py <out mod_override folder>

Output: art\revived\unit_bans.tga, one 256 x 256 atlas, and preview_unit_bans.png. The pieces are
declared in setupwin.xml's SETUPWIN block as IFACETEX "ub_<piece>" (tex_lines below), with the
unit icon pages and the window's frame (the same steel IFACEBORDER pieces as the setup screen's
panels); unitbans.c finds them with the SetupWin's find_tex / find_border:

  ub_plate        brushed steel, seamless: tiled over the whole window
  ub_band         dark wash, stretched: the title band
  ub_well         darker wash, stretched: under each column
  ub_cell         a unit's slot, a dark recess with a light lip, stretched to the icon plus 3 px
  ub_ban          a red X over the icon of a unit that is off (61 x 39, the icon's size)
  ub_glow         a gold wash, stretched: under what the mouse is on
  ub_line         a gold divider, stretched
  ub_x_off / _hover   the column's round X: gold ring, gold X (some of the column is on)
  ub_x_on / _hover    the same, red (the whole column is off)
  ub_tab / ub_tab_on  the two pages' tabs, stretched across
  ub_button / _hover  Allow all

Everything that lies on the plate is in this one atlas on purpose: inside a window the game draws
its quads texture by texture, and quads of one texture keep the order they were added in.
"""
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_assist_panel as A  # noqa: E402  (the drawing helpers and colours)

ATLAS = "art\\revived\\unit_bans.tga"
SIZE = 256
PLATE = 128
TAB_W, TAB_H = 120, 26
X = 24

PIECES = {
    "plate": (0, 0, PLATE, PLATE),
    "cell": (130, 0, 66, 44),
    "x_off": (198, 0, X, X),
    "x_off_hover": (224, 0, X, X),
    "x_on": (198, 26, X, X),
    "x_on_hover": (224, 26, X, X),
    "glow": (130, 46, 16, 16),
    "well": (148, 46, 16, 16),
    "band": (166, 46, 16, 16),
    "line": (184, 46, 32, 4),
    "ban": (0, 130, 61, 39),
    "tab": (64, 130, TAB_W, TAB_H),
    "tab_on": (64, 158, TAB_W, TAB_H),
    "button": (64, 186, TAB_W, TAB_H),
    "button_hover": (64, 214, TAB_W, TAB_H),
}


def plate():
    """Brushed dark steel that tiles: the streaks wrap round both edges."""
    rng = np.random.default_rng(7)
    n = PLATE
    rows = rng.normal(0, 1, n)
    rows = (np.roll(rows, 1) + rows + np.roll(rows, -1)) / 3
    fine = rng.normal(0, 1, (n, n))
    k = 6
    fine = sum(np.roll(fine, s, axis=1) for s in range(-k, k + 1)) / (2 * k + 1)
    light = rows[:, None] * 0.010 + fine * 0.016
    base = np.array([0.105, 0.116, 0.132])
    rgb = np.clip(base[None, None, :] + light[..., None], 0, 1)
    a = np.ones((n, n, 1))
    return Image.fromarray((np.dstack([rgb, a]) * 255 + 0.5).astype(np.uint8), "RGBA")


def cell():
    w, h = PIECES["cell"][2:]
    c = A.Canvas(w, h)
    s = A.rrect_sdf(c.x, c.y, 0.5, 0.5, w - 0.5, h - 0.5, 4)
    c.paint(A.cover(s), (0.025, 0.028, 0.032))
    inner = np.clip(1 - (c.y - 0.5) / (h * 0.35), 0, 1) ** 2
    c.paint(A.cover(s) * inner, (0, 0, 0), 0.7)
    lip = A.cover(np.abs(s) - 0.6) * np.clip((c.y - h / 2) / (h / 2), 0, 1)
    c.paint(lip, (0.50, 0.52, 0.55), 0.6)
    top = A.cover(np.abs(s) - 0.6) * np.clip((h / 2 - c.y) / (h / 2), 0, 1)
    c.paint(top, (0, 0, 0), 0.85)
    return c.image()


def round_x(red, hover):
    c = A.Canvas(X, X)
    r = X / 2 - 1
    s = A.circle_sdf(c.x, c.y, X / 2, X / 2, r)
    c.paint(A.cover(A.circle_sdf(c.x, c.y, X / 2 + 0.5, X / 2 + 0.8, r)), (0, 0, 0), 0.5)
    if red:
        body = ((0.62, 0.12, 0.09), (0.30, 0.03, 0.03)) if hover else ((0.48, 0.08, 0.06), (0.20, 0.02, 0.02))
    else:
        body = ((0.20, 0.17, 0.08), (0.05, 0.04, 0.02)) if hover else ((0.10, 0.10, 0.11), (0.0, 0.0, 0.0))
    c.paint(A.cover(s), A.vgrad(c, *body))
    c.paint(A.cover(s) * A.cover(-(s + 1.6)), A.GOLD_HI if hover else A.GOLD)
    k = X * 0.17
    d = np.minimum(A.seg_sdf(c.x, c.y, X / 2 - k, X / 2 - k, X / 2 + k, X / 2 + k),
                   A.seg_sdf(c.x, c.y, X / 2 - k, X / 2 + k, X / 2 + k, X / 2 - k))
    c.paint(A.cover(d - 2.0), (0, 0, 0), 0.5)
    c.paint(A.cover(d - 1.4), (1.0, 0.92, 0.80) if red else (A.GOLD_HI if hover else A.GOLD))
    return c.image()


def ban():
    w, h = PIECES["ban"][2:]
    c = A.Canvas(w, h)
    c.paint(np.ones_like(c.x), (0.25, 0.0, 0.0), 0.35)  # the icon goes dark red
    m = 7
    d = np.minimum(A.seg_sdf(c.x, c.y, m + 6, m - 2, w - m - 6, h - m + 2),
                   A.seg_sdf(c.x, c.y, m + 6, h - m + 2, w - m - 6, m - 2))
    c.paint(A.cover(d - 4.2), (0, 0, 0), 0.7)
    u = np.clip(c.y / h, 0, 1)[..., None]
    c.paint(A.cover(d - 3.0), np.array([0.95, 0.25, 0.18]) * (1 - u) + np.array([0.65, 0.08, 0.05]) * u)
    return c.image()


def bar(top, bottom, rim, rim_alpha):
    """A tab or a button: a rounded bar with a thin rim."""
    c = A.Canvas(TAB_W, TAB_H)
    s = A.rrect_sdf(c.x, c.y, 0.5, 0.5, TAB_W - 0.5, TAB_H - 0.5, 5)
    c.paint(A.cover(s), A.vgrad(c, top, bottom))
    gloss = A.cover(A.rrect_sdf(c.x, c.y, 2, 2, TAB_W - 2, TAB_H * 0.45, 4))
    c.paint(gloss, (1, 1, 1), 0.05)
    c.paint(A.cover(np.abs(s + 0.8) - 0.8), rim, rim_alpha)
    return c.image()


def build():
    pics = {
        "plate": plate(),
        "cell": cell(),
        "x_off": round_x(False, False),
        "x_off_hover": round_x(False, True),
        "x_on": round_x(True, False),
        "x_on_hover": round_x(True, True),
        "glow": A.flat(16, 16, [(230, 190, 60, 60)] * 16),
        "well": A.flat(16, 16, [(0, 0, 0, 110)] * 16),
        "band": A.flat(16, 16, [(0, 0, 0, 150)] * 16),
        "line": A.flat(32, 4, [(230, 190, 60, 220), (230, 190, 60, 220), (0, 0, 0, 150), (0, 0, 0, 150)]),
        "ban": ban(),
        "tab": bar((0.13, 0.14, 0.16), (0.07, 0.075, 0.085), (0.45, 0.47, 0.50), 0.5),
        "tab_on": bar((0.24, 0.21, 0.12), (0.12, 0.10, 0.05), A.GOLD, 0.95),
        "button": bar((0.16, 0.17, 0.19), (0.08, 0.085, 0.095), A.GOLD, 0.8),
        "button_hover": bar((0.30, 0.26, 0.13), (0.15, 0.12, 0.05), A.GOLD_HI, 1.0),
    }
    atlas = Image.new("RGBA", (SIZE, SIZE))
    for name, (x, y, w, h) in PIECES.items():
        assert pics[name].size == (w, h), name
        atlas.paste(pics[name], (x, y))
    return atlas


def tex_lines():
    """For setupwin.xml's SETUPWIN block: this atlas, the unit icon pages (the in-game ones are not
    loaded in the setup screen) and the window's frame, the setup panels' own steel pieces."""
    lines = ['<IFACETEXLOAD id="TEX_REVIVED_UNIT_BANS" file=".\\%s" pixformat="PF_32"/>' % ATLAS]
    lines += ['<IFACETEX name="ub_%s" texture="TEX_REVIVED_UNIT_BANS" left="%d" top="%d" width="%d" height="%d"/>'
              % (name, x, y, w, h) for name, (x, y, w, h) in PIECES.items()]
    pages = ["art\\iface_units_01.tga", "art\\iface_units_02.tga", "art\\iface_units_03.tga",
             "art\\romania_units_0.tga"]
    for i, f in enumerate(pages):
        lines.append('<IFACETEXLOAD id="TEX_UB_ICONS_%d" file=".\\%s" pixformat="PF_32"/>' % (i, f))
        lines.append('<IFACETEX name="ub_icons_%d" texture="TEX_UB_ICONS_%d" left="0" top="0" width="61" height="39"/>'
                     % (i, i))
    lines += ['<IFACEBORDER name="ub_border">',
              '  <IFACETEX name="corner0" texture="TEX_GAMESPY" left="209" top="0" width="18" height="18"/>',
              '  <IFACETEX name="corner1" texture="TEX_GAMESPY" left="494" top="0" width="18" height="18"/>',
              '  <IFACETEX name="corner2" texture="TEX_GAMESPY" left="494" top="285" width="18" height="18"/>',
              '  <IFACETEX name="corner3" texture="TEX_GAMESPY" left="209" top="285" width="18" height="18"/>',
              '  <IFACETEX name="edge0" texture="TEX_GAMESPY" left="228" top="0" width="265" height="18"/>',
              '  <IFACETEX name="edge1" texture="TEX_GAMESPY" left="494" top="19" width="18" height="265"/>',
              '  <IFACETEX name="edge2" texture="TEX_GAMESPY" left="228" top="285" width="265" height="18"/>',
              '  <IFACETEX name="edge3" texture="TEX_GAMESPY" left="209" top="19" width="18" height="265"/>',
              '</IFACEBORDER>']
    return lines


def preview(atlas, game):
    """A few columns roughly as unitbans.c draws them, with real icons when the game is there."""
    W, H = 420, 300
    p = Image.new("RGBA", (W, H))
    tile = atlas.crop((0, 0, PLATE, PLATE))
    for y in range(0, H, PLATE):
        for x in range(0, W, PLATE):
            p.alpha_composite(tile, (x, y))
    piece = lambda n: atlas.crop((PIECES[n][0], PIECES[n][1], PIECES[n][0] + PIECES[n][2],
                                  PIECES[n][1] + PIECES[n][3]))
    p.alpha_composite(piece("band").resize((W, 40)), (0, 0))
    p.alpha_composite(piece("line").resize((W - 20, 2)), (10, 40))
    p.alpha_composite(piece("x_off"), (W - 34, 8))
    p.alpha_composite(piece("tab_on"), (14, 50))
    p.alpha_composite(piece("tab").resize((110, TAB_H)), (140, 50))
    p.alpha_composite(piece("button_hover"), (W - 134, 50))
    icons = None
    if game:
        f = os.path.join(game, "art", "iface_units_01.tga")
        if os.path.exists(f):
            icons = Image.open(f).convert("RGBA")
    for col in range(5):
        x0 = 14 + col * 81
        p.alpha_composite(piece("well").resize((75, 210)), (x0, 86))
        p.alpha_composite(piece("x_on" if col == 3 else "x_off_hover" if col == 1 else "x_off"), (x0 + 25, 89))
        for row in range(4):
            y0 = 118 + row * 44
            p.alpha_composite(piece("cell").resize((67, 41)), (x0 + 4, y0))
            if icons:
                p.alpha_composite(icons.crop((((col + row) % 8) * 62, row * 40, ((col + row) % 8) * 62 + 61,
                                              row * 40 + 39)).resize((61, 35)), (x0 + 7, y0 + 3))
            if col == 3 or (col == 2 and row == 3):
                p.alpha_composite(piece("ban").resize((61, 35)), (x0 + 7, y0 + 3))
            if col == 0 and row == 1:
                p.alpha_composite(piece("glow").resize((67, 41)), (x0 + 4, y0))
    return p


def write_tga(img, path):
    """Uncompressed 32-bit, bottom-up, as build_menu_theme.write_tga (not imported: that module
    imports the others, which import it back)."""
    import struct
    os.makedirs(os.path.dirname(path), exist_ok=True)
    w, h = img.size
    a = np.asarray(img.convert("RGBA"))[::-1]
    with open(path, "wb") as f:
        f.write(bytes([0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0]) + struct.pack("<HHBB", w, h, 32, 8) +
                a[..., [2, 1, 0, 3]].tobytes())
    print(f"{path}: {w}x{h}")


def main(out, game=None):
    atlas = build()
    write_tga(atlas, os.path.join(out, ATLAS))
    prev = preview(atlas, game)
    prev.resize((prev.width * 2, prev.height * 2), Image.LANCZOS).save(
        os.path.join(HERE, "..", "preview_unit_bans.png"))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else
         r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations")
