r"""The art of the allied requests panel (camera-smooth\src\allyreq.c): a computer ally asks you
for something, with its nation's medallion, the request, a countdown and Accept / Decline.
Drawn here (no source picture) in the theme's look, as the AI assistant panel
(build_assist_panel.py); the buttons are the main menu's steel plate (build_menu_theme.py,
plate_states).

    python build_ally_panel.py <out mod_override folder>

Output: art\revived\ally_panel.tga, one 512 x 256 atlas, and preview_ally_panel.png.
build_menu_theme.py declares the pieces in interface.xml, in the IFACEMAINBASE block, as IFACETEX
"ally_<piece>" (tex_lines below), together with the nation medallions of build_flags.py as
"ally_flag_<tribe>" (the setup screen's own flag names are not loaded in a game). allyreq.c
draws the pieces at these exact sizes:

  ally_back            the window, PANEL_W x PANEL_H: steel plate, bevelled frame, rivets,
                       title band, the medallion's round seat
  ally_btn / _hover / _down   a button, BTN_W x BTN_H, the main menu's steel plate
  ally_bar             the countdown's recessed track, BAR_W x BAR_H
  ally_fill            the countdown's gold fill, stretched to the time left
"""
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_assist_panel as A  # noqa: E402  (the drawing helpers and colours)
import build_flags  # noqa: E402

ATLAS = "art\\revived\\ally_panel.tga"
FLAGS_ID = "TEX_REVIVED_ALLY_FLAGS"

# the same numbers are in allyreq.c
PANEL_W, PANEL_H, TITLE_H = 460, 178, 34
SEAT_X, SEAT_Y, SEAT = 18, 48, 72      # the medallion's round seat (the flag is drawn 64 px inside)
BTN_W, BTN_H = 132, 30
BTN_L, BTN_M, BTN_R = 15, 102, 15
BAR_W, BAR_H = 346, 10

PIECES = {
    "back": (0, 0, PANEL_W, PANEL_H),
    "btn": (0, 180, BTN_W, BTN_H),
    "btn_hover": (134, 180, BTN_W, BTN_H),
    "btn_down": (268, 180, BTN_W, BTN_H),
    "bar": (0, 214, BAR_W, BAR_H),
    "fill": (350, 214, 16, 6),
}


def back():
    W, H = PANEL_W, PANEL_H
    c = A.Canvas(W, H)
    outer = A.rrect_sdf(c.x, c.y, 0, 0, W, H, 7)
    base = A.vgrad(c, (0.135, 0.150, 0.170), (0.075, 0.082, 0.095))
    base = np.clip(base + A.brushed(c, 5, 0.012)[..., None], 0, 1)
    c.paint(A.cover(outer), base, 0.96)
    vx = np.abs(c.x / W - 0.5) * 2
    vy = np.abs(c.y / H - 0.5) * 2
    c.paint(A.cover(outer) * np.clip(np.maximum(vx, vy) ** 3, 0, 1) * 0.35, (0, 0, 0))
    # title band
    band = A.cover(A.rrect_sdf(c.x, c.y, 4, 4, W - 4, TITLE_H, 4))
    c.paint(band, A.vgrad(c, (0.06, 0.065, 0.075), (0.03, 0.033, 0.04), 4, TITLE_H), 0.9)
    c.paint(band * np.exp(-((c.y - 9) / 5) ** 2), (1, 1, 1), 0.06)
    line = A.cover(np.abs(c.x - W / 2) - (W / 2 - 10))
    c.paint(A.cover(np.abs(c.y - (TITLE_H + 0.5)) - 0.5) * line, A.GOLD, 0.85)
    c.paint(A.cover(np.abs(c.y - (TITLE_H + 1.5)) - 0.5) * line, (0, 0, 0), 0.6)
    # the medallion's seat: a dark round recess with a gold ring
    cx, cy, r = SEAT_X + SEAT / 2, SEAT_Y + SEAT / 2, SEAT / 2
    s = A.circle_sdf(c.x, c.y, cx, cy, r)
    c.paint(A.cover(A.circle_sdf(c.x, c.y, cx + 0.6, cy + 1.0, r)), (0, 0, 0), 0.55)
    c.paint(A.cover(s), A.vgrad(c, (0.02, 0.022, 0.026), (0.07, 0.075, 0.085), SEAT_Y, SEAT_Y + SEAT))
    A.gold_ring(c, s, 2.4)
    c.paint(A.cover(np.abs(s + 2.9) - 0.5), (0, 0, 0), 0.5)
    # bevelled frame, outline, thin gold line inside
    frame = A.cover(outer) * A.cover(-(outer + 3))
    u = np.clip((c.x / W + c.y / H) / 2, 0, 1)[..., None]
    c.paint(frame, np.array([0.55, 0.57, 0.60]) * (1 - u) + np.array([0.12, 0.13, 0.15]) * u)
    c.paint(A.cover(outer) * A.cover(-(outer + 1)), (0, 0, 0), 0.8)
    inner = A.rrect_sdf(c.x, c.y, 6, 6, W - 6, H - 6, 4)
    c.paint(A.cover(np.abs(inner) - 0.5), A.GOLD, 0.55)
    for x, y in ((12, 12), (W - 12, 12), (12, H - 12), (W - 12, H - 12)):
        A.rivet(c, x, y, 3.2)
    return c.image()


def buttons():
    from build_menu_theme import element, plate_states
    normal, hover, pressed = plate_states(element(11), BTN_H, BTN_L, BTN_M, BTN_R)
    return normal, hover, pressed


def build():
    normal, hover, pressed = buttons()
    pics = {
        "back": back(),
        "btn": normal,
        "btn_hover": hover,
        "btn_down": pressed,
        "bar": A.recess(BAR_W, BAR_H, BAR_H / 2, 4),
        "fill": A.flat(16, 6, [(255, 228, 140, 255), (246, 212, 112, 255), (236, 196, 84, 255),
                               (222, 180, 60, 255), (196, 152, 40, 255), (160, 118, 30, 255)]),
    }
    atlas = Image.new("RGBA", (512, 256))
    for name, (x, y, w, h) in PIECES.items():
        assert pics[name].size == (w, h), (name, pics[name].size)
        atlas.paste(pics[name], (x, y))
    return atlas


def tex_lines():
    lines = ['<IFACETEXLOAD id="TEX_REVIVED_ALLY" file=".\\%s" pixformat="PF_32"/>' % ATLAS]
    lines += [f'<IFACETEX name="ally_{name}" texture="TEX_REVIVED_ALLY" left="{x}" top="{y}" width="{w}" height="{h}"/>'
              for name, (x, y, w, h) in PIECES.items()]
    # the nation medallions (build_flags.py's atlas), by tribe index; a missing one is an empty cell
    lines.append('<IFACETEXLOAD id="%s" file=".\\%s" pixformat="PF_32"/>' % (FLAGS_ID, build_flags.ATLAS))
    for t in range(build_flags.NUM_TRIBES):
        x, y, w, h = build_flags.slot_rect(t)
        lines.append(f'<IFACETEX name="ally_flag_{t}" texture="{FLAGS_ID}" left="{x}" top="{y}" width="{w}" height="{h}"/>')
    return lines


def preview(atlas):
    """The panel put together roughly as allyreq.c draws it (no text), on a game-like grey."""
    p = Image.new("RGBA", (PANEL_W + 40, PANEL_H + 40), (58, 66, 52, 255))
    piece = lambda n: atlas.crop((PIECES[n][0], PIECES[n][1], PIECES[n][0] + PIECES[n][2], PIECES[n][1] + PIECES[n][3]))
    ox, oy = 20, 20
    p.alpha_composite(piece("back"), (ox, oy))
    p.alpha_composite(piece("bar"), (ox + 100, oy + 124))
    p.alpha_composite(piece("fill").resize((220, 6)), (ox + 102, oy + 126))
    p.alpha_composite(piece("btn_hover"), (ox + PANEL_W - 18 - 2 * BTN_W - 12, oy + PANEL_H - 18 - BTN_H))
    p.alpha_composite(piece("btn"), (ox + PANEL_W - 18 - BTN_W, oy + PANEL_H - 18 - BTN_H))
    flags = os.path.join(HERE, "..", "preview_flags.png")
    if os.path.exists(flags):
        f = Image.open(flags).convert("RGBA")
        f = f.resize((f.width // 3, f.height // 3), Image.NEAREST).crop((64 * 3, 0, 64 * 4, 64))
        p.alpha_composite(f, (ox + SEAT_X + 4, oy + SEAT_Y + 4))
    return p


def main(out):
    from build_menu_theme import write_tga
    atlas = build()
    write_tga(atlas, os.path.join(out, ATLAS))
    prev = preview(atlas)
    prev.resize((prev.width * 2, prev.height * 2), Image.LANCZOS).save(os.path.join(HERE, "..", "preview_ally_panel.png"))


if __name__ == "__main__":
    main(sys.argv[1])
