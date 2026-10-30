r"""The AI assistant panel's art (camera-smooth\src\assist_panel.c), drawn here (no source picture)
in the theme's look: dark gunmetal, riveted frame, gold rims like the round qol buttons.

    python build_assist_panel.py <out mod_override folder>

Output: art\revived\assist_panel.tga, one 512 x 512 atlas. build_menu_theme.py declares its
pieces in interface.xml, in the IFACEMAINBASE block, as IFACETEX "assist_<piece>" (tex_lines
below); assist_panel.c draws them at these exact sizes, so the sizes here and there must match:

  assist_back            the window, PANEL_W x PANEL_H: steel plate, bevelled frame, rivets,
                         title band
  assist_track           the control switch's recessed track, TRACK_W x TRACK_H
  assist_plate_full      the switch's sliding plate, PLATE_W x PLATE_H: green (full control),
  assist_plate_custom    amber (on, with some kinds unticked), red (no control)
  assist_plate_none
  assist_box_off / _on / _hover   round checkboxes, BOX x BOX
  assist_close / _hover  the close button, BOX x BOX
  assist_knob            the reserve slider's knob, KNOB x KNOB
  assist_slider          the slider's recessed track, SLIDER_W x SLIDER_H
  assist_fill            the slider's gold fill, stretched to the value
  assist_line            a gold divider (gold over a dark line), stretched to its length
  assist_glow            the faint gold wash under the row under the mouse, stretched

Everything is in one atlas on purpose: inside a window the game draws its quads texture by
texture (camera-smooth\ENGINE_NOTES.md, "Draw order"), so solid fills could end up over or
under the pieces in any order; quads of one texture keep the order they were added in.
"""
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ATLAS = "art\\revived\\assist_panel.tga"
SS = 4  # supersampling

PANEL_W, PANEL_H, TITLE_H = 320, 352, 36
TRACK_W, TRACK_H = 288, 40
PLATE_W, PLATE_H = 140, 34
BOX = 24
KNOB = 22
SLIDER_W, SLIDER_H = 272, 14

GOLD = np.array([230, 190, 60]) / 255
GOLD_HI = np.array([255, 228, 140]) / 255
GOLD_LO = np.array([150, 108, 28]) / 255

# where each piece sits in the atlas (2 px gaps, so filtering does not bleed)
PIECES = {
    "back": (0, 0, PANEL_W, PANEL_H),
    "track": (0, 370, TRACK_W, TRACK_H),
    "slider": (0, 414, SLIDER_W, SLIDER_H),
    "plate_full": (324, 0, PLATE_W, PLATE_H),
    "plate_custom": (324, 38, PLATE_W, PLATE_H),
    "plate_none": (324, 76, PLATE_W, PLATE_H),
    "box_off": (324, 114, BOX, BOX),
    "box_on": (352, 114, BOX, BOX),
    "box_hover": (380, 114, BOX, BOX),
    "close": (408, 114, BOX, BOX),
    "close_hover": (436, 114, BOX, BOX),
    "knob": (464, 114, KNOB, KNOB),
    "fill": (324, 142, 16, 8),
    "line": (344, 142, 32, 4),
    "glow": (380, 142, 32, 16),
}


# ---- drawing on float canvases (RGB 0..1 plus alpha), in output pixels times SS ----------

def coords(w, h):
    """Pixel centres in output pixels."""
    x = (np.arange(w * SS) + 0.5) / SS
    y = (np.arange(h * SS) + 0.5) / SS
    return np.meshgrid(x, y)


def rrect_sdf(x, y, l, t, r, b, rad):
    """Signed distance to a rounded rectangle (negative inside)."""
    cx, cy = (l + r) / 2, (t + b) / 2
    hx, hy = (r - l) / 2 - rad, (b - t) / 2 - rad
    dx, dy = np.abs(x - cx) - hx, np.abs(y - cy) - hy
    out = np.hypot(np.maximum(dx, 0), np.maximum(dy, 0))
    return out + np.minimum(np.maximum(dx, dy), 0) - rad


def circle_sdf(x, y, cx, cy, rad):
    return np.hypot(x - cx, y - cy) - rad


def cover(sdf):
    """Coverage of a shape from its distance, antialiased over one supersample."""
    return np.clip(0.5 - sdf * SS, 0, 1)


def seg_sdf(x, y, ax, ay, bx, by):
    px, py, vx, vy = x - ax, y - ay, bx - ax, by - ay
    h = np.clip((px * vx + py * vy) / (vx * vx + vy * vy), 0, 1)
    return np.hypot(px - vx * h, py - vy * h)


class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.rgb = np.zeros((h * SS, w * SS, 3))
        self.a = np.zeros((h * SS, w * SS))
        self.x, self.y = coords(w, h)

    def paint(self, mask, color, alpha=1.0):
        """Paints colour (an RGB triple or an H x W x 3 array) where mask (0..1) is set."""
        m = np.clip(mask * alpha, 0, 1)[..., None]
        color = np.broadcast_to(np.asarray(color, dtype=float), self.rgb.shape)
        self.rgb = self.rgb * (1 - m) + color * m
        self.a = self.a + (1 - self.a) * m[..., 0]

    def image(self):
        """Downsampled, premultiplied while filtering, as an RGBA PIL image."""
        pre = np.dstack([self.rgb * self.a[..., None], self.a])
        h, w = self.h, self.w
        small = pre.reshape(h, SS, w, SS, 4).mean(axis=(1, 3))
        a = small[..., 3:]
        rgb = np.where(a > 1e-4, small[..., :3] / np.maximum(a, 1e-4), 0)
        out = np.dstack([np.clip(rgb, 0, 1), a])
        out[out[..., 3] < 2 / 255] = 0
        return Image.fromarray((out * 255 + 0.5).astype(np.uint8), "RGBA")


def vgrad(c, top, bottom, t=None, b=None):
    """Vertical gradient between two colours over rows t..b (output pixels)."""
    t = 0 if t is None else t
    b = c.h if b is None else b
    u = np.clip((c.y - t) / max(b - t, 1e-6), 0, 1)[..., None]
    return np.asarray(top) * (1 - u) + np.asarray(bottom) * u


def brushed(c, seed, strength):
    """Horizontal brushed-metal streaks, as a brightness offset per pixel."""
    rng = np.random.default_rng(seed)
    rows = rng.normal(0, 1, c.h * SS)
    rows = np.convolve(rows, np.ones(3) / 3, mode="same")
    fine = rng.normal(0, 1, (c.h * SS, c.w * SS))
    k = np.ones(SS * 6) / (SS * 6)
    fine = np.apply_along_axis(lambda r: np.convolve(r, k, mode="same"), 1, fine)
    return (rows[:, None] * 0.6 + fine * 0.8) * strength


def rivet(c, cx, cy, rad):
    c.paint(cover(circle_sdf(c.x, c.y, cx + 0.6, cy + 0.8, rad)), (0, 0, 0), 0.6)  # shadow
    body = cover(circle_sdf(c.x, c.y, cx, cy, rad))
    u = np.clip(((c.x - cx) + (c.y - cy)) / (2 * rad) + 0.5, 0, 1)[..., None]
    c.paint(body, np.array([0.62, 0.64, 0.67]) * (1 - u) + np.array([0.16, 0.17, 0.19]) * u)
    c.paint(cover(circle_sdf(c.x, c.y, cx - rad * 0.35, cy - rad * 0.35, rad * 0.32)), (0.95, 0.95, 0.95), 0.7)


def gold_ring(c, sdf, width, light=True):
    """A gold band of `width` just inside the shape's edge, lit from the top."""
    band = cover(sdf) * cover(-(sdf + width))
    u = np.clip(c.y / c.h, 0, 1)[..., None]
    top, bottom = (GOLD_HI, GOLD_LO) if light else (GOLD, GOLD_LO)
    c.paint(band, top * (1 - u) + bottom * u)


# ---- the pieces --------------------------------------------------------------------------

def back():
    c = Canvas(PANEL_W, PANEL_H)
    W, H = PANEL_W, PANEL_H
    outer = rrect_sdf(c.x, c.y, 0, 0, W, H, 7)
    # steel plate, dark, brushed, a little lighter at the top
    base = vgrad(c, (0.135, 0.150, 0.170), (0.075, 0.082, 0.095))
    base = np.clip(base + brushed(c, 1, 0.012)[..., None], 0, 1)
    c.paint(cover(outer), base, 0.96)
    # vignette towards the edges
    vx = np.abs(c.x / W - 0.5) * 2
    vy = np.abs(c.y / H - 0.5) * 2
    vig = np.clip(np.maximum(vx, vy) ** 3, 0, 1) * 0.35
    c.paint(cover(outer) * vig, (0, 0, 0))
    # title band: darker, with a soft sheen
    band = cover(rrect_sdf(c.x, c.y, 4, 4, W - 4, TITLE_H, 4))
    c.paint(band, vgrad(c, (0.06, 0.065, 0.075), (0.03, 0.033, 0.04), 4, TITLE_H), 0.9)
    sheen = np.exp(-((c.y - 9) / 5) ** 2)
    c.paint(band * sheen, (1, 1, 1), 0.06)
    # gold line under the title, with a dark line below it
    c.paint(cover(np.abs(c.y - (TITLE_H + 0.5)) - 0.5) * cover(np.abs(c.x - W / 2) - (W / 2 - 10)), GOLD, 0.85)
    c.paint(cover(np.abs(c.y - (TITLE_H + 1.5)) - 0.5) * cover(np.abs(c.x - W / 2) - (W / 2 - 10)), (0, 0, 0), 0.6)
    # bevelled steel frame: light top-left, dark bottom-right
    frame = cover(outer) * cover(-(outer + 3))
    u = np.clip((c.x / W + c.y / H) / 2, 0, 1)[..., None]
    c.paint(frame, np.array([0.55, 0.57, 0.60]) * (1 - u) + np.array([0.12, 0.13, 0.15]) * u)
    c.paint(cover(outer) * cover(-(outer + 1)), (0, 0, 0), 0.8)  # outline
    # thin gold line inside the frame
    inner = rrect_sdf(c.x, c.y, 6, 6, W - 6, H - 6, 4)
    c.paint(cover(np.abs(inner) - 0.5), GOLD, 0.55)
    # rivets in the corners and halfway down the sides
    for cx, cy in ((12, 12), (W - 12, 12), (12, H - 12), (W - 12, H - 12), (12, H / 2), (W - 12, H / 2)):
        rivet(c, cx, cy, 3.2)
    return c.image()


def recess(w, h, rad, seed):
    """A dark rounded groove cut into the plate: inner shadow at the top, light lip below."""
    c = Canvas(w, h)
    s = rrect_sdf(c.x, c.y, 0.5, 0.5, w - 0.5, h - 0.5, rad)
    c.paint(cover(s), (0.02, 0.022, 0.026))
    inner = np.clip(1 - (c.y - 0.5) / (h * 0.45), 0, 1) ** 2  # shadow from the top edge
    c.paint(cover(s) * inner, (0, 0, 0), 0.8)
    c.paint(cover(s) * np.clip(-s / 3, 0, 1) * (1 - np.clip(-s / 3, 0, 1)), (0.25, 0.26, 0.28), 0.25)
    lip = cover(np.abs(s) - 0.6) * np.clip((c.y - h / 2) / (h / 2), 0, 1)
    c.paint(lip, (0.55, 0.57, 0.60), 0.55)
    top = cover(np.abs(s) - 0.6) * np.clip((h / 2 - c.y) / (h / 2), 0, 1)
    c.paint(top, (0, 0, 0), 0.8)
    return c.image()


def plate(top, bottom):
    """The switch's sliding plate: a coloured, glossy bar with a gold rim and grip lines."""
    c = Canvas(PLATE_W, PLATE_H)
    W, H = PLATE_W, PLATE_H
    s = rrect_sdf(c.x, c.y, 0.5, 0.5, W - 0.5, H - 0.5, 8)
    c.paint(cover(s), vgrad(c, top, bottom))
    gloss = cover(rrect_sdf(c.x, c.y, 3, 2.5, W - 3, H * 0.48, 6))
    c.paint(gloss * np.clip(1 - c.y / (H * 0.5), 0, 1), (1, 1, 1), 0.22)
    gold_ring(c, s, 2.0)
    c.paint(cover(np.abs(s + 2.5) - 0.5), (0, 0, 0), 0.35)  # dark line inside the rim
    return c.image()


def box(on, hover):
    """Round checkbox like the qol buttons: black, gold ring; a gold tick when on."""
    c = Canvas(BOX, BOX)
    r = BOX / 2 - 1
    s = circle_sdf(c.x, c.y, BOX / 2, BOX / 2, r)
    c.paint(cover(circle_sdf(c.x, c.y, BOX / 2 + 0.5, BOX / 2 + 0.8, r)), (0, 0, 0), 0.5)
    c.paint(cover(s), vgrad(c, (0.10, 0.10, 0.11), (0.0, 0.0, 0.0)))
    if hover:
        c.paint(cover(s) * np.exp(-(circle_sdf(c.x, c.y, BOX / 2, BOX / 2, 0) / (r * 0.8)) ** 2), GOLD, 0.25)
    ring = 2.0
    band = cover(s) * cover(-(s + ring))
    c.paint(band, GOLD_HI if hover else GOLD)
    c.paint(cover(np.abs(s + ring + 0.4) - 0.4), (0, 0, 0), 0.6)
    if on:
        a, m, b = (BOX * 0.29, BOX * 0.52), (BOX * 0.44, BOX * 0.67), (BOX * 0.72, BOX * 0.33)
        d = np.minimum(seg_sdf(c.x, c.y, *a, *m), seg_sdf(c.x, c.y, *m, *b))
        c.paint(cover(d - 2.2), (0, 0, 0), 0.6)
        u = np.clip(c.y / BOX, 0, 1)[..., None]
        c.paint(cover(d - 1.5), GOLD_HI * (1 - u) + GOLD * u)
    return c.image()


def close(hover):
    c = Canvas(BOX, BOX)
    r = BOX / 2 - 1
    s = circle_sdf(c.x, c.y, BOX / 2, BOX / 2, r)
    body = ((0.42, 0.08, 0.06), (0.18, 0.02, 0.02)) if hover else ((0.10, 0.10, 0.11), (0.0, 0.0, 0.0))
    c.paint(cover(s), vgrad(c, *body))
    c.paint(cover(s) * cover(-(s + 1.6)), GOLD_HI if hover else GOLD)
    k = BOX * 0.17
    d = np.minimum(seg_sdf(c.x, c.y, BOX / 2 - k, BOX / 2 - k, BOX / 2 + k, BOX / 2 + k),
                   seg_sdf(c.x, c.y, BOX / 2 - k, BOX / 2 + k, BOX / 2 + k, BOX / 2 - k))
    c.paint(cover(d - 1.3), GOLD_HI if hover else GOLD)
    return c.image()


def knob():
    c = Canvas(KNOB, KNOB)
    r = KNOB / 2 - 1.5
    c.paint(cover(circle_sdf(c.x, c.y, KNOB / 2 + 0.6, KNOB / 2 + 1, r)), (0, 0, 0), 0.55)
    s = circle_sdf(c.x, c.y, KNOB / 2, KNOB / 2, r)
    d = np.clip(np.hypot(c.x - KNOB * 0.38, c.y - KNOB * 0.34) / (r * 1.6), 0, 1)[..., None]
    c.paint(cover(s), GOLD_HI * (1 - d) + GOLD_LO * d)
    c.paint(cover(np.abs(s + 0.5) - 0.5), (0.35, 0.22, 0.04), 0.8)
    c.paint(cover(circle_sdf(c.x, c.y, KNOB / 2, KNOB / 2, r * 0.38)), (0.45, 0.30, 0.06), 0.5)
    c.paint(cover(circle_sdf(c.x, c.y, KNOB * 0.38, KNOB * 0.33, r * 0.22)), (1, 1, 1), 0.55)
    return c.image()


def flat(w, h, rows):
    """A plain piece made of rows of (r, g, b, a) colours, repeated across."""
    a = np.zeros((h, w, 4), dtype=np.uint8)
    for k, c in enumerate(rows):
        a[k, :] = c
    return Image.fromarray(a, "RGBA")


def build():
    pics = {
        "back": back(),
        "track": recess(TRACK_W, TRACK_H, 11, 2),
        "slider": recess(SLIDER_W, SLIDER_H, SLIDER_H / 2, 3),
        "plate_full": plate((0.27, 0.62, 0.30), (0.10, 0.33, 0.13)),
        "plate_custom": plate((0.80, 0.56, 0.16), (0.45, 0.29, 0.06)),
        "plate_none": plate((0.62, 0.20, 0.17), (0.32, 0.07, 0.06)),
        "box_off": box(False, False),
        "box_on": box(True, False),
        "box_hover": box(False, True),
        "close": close(False),
        "close_hover": close(True),
        "knob": knob(),
        "fill": flat(16, 8, [(255, 228, 140, 255), (246, 212, 112, 255), (236, 196, 84, 255), (230, 190, 60, 255),
                             (214, 172, 50, 255), (196, 152, 40, 255), (176, 130, 34, 255), (150, 108, 28, 255)]),
        "line": flat(32, 4, [(230, 190, 60, 200), (230, 190, 60, 200), (0, 0, 0, 140), (0, 0, 0, 140)]),
        "glow": flat(32, 16, [(230, 190, 60, 22)] * 16),
    }
    atlas = Image.new("RGBA", (512, 512))
    for name, (x, y, w, h) in PIECES.items():
        assert pics[name].size == (w, h), name
        atlas.paste(pics[name], (x, y))
    return atlas


def tex_lines():
    return ['<IFACETEXLOAD id="TEX_REVIVED_ASSIST" file=".\\%s" pixformat="PF_32"/>' % ATLAS] + [
        f'<IFACETEX name="assist_{name}" texture="TEX_REVIVED_ASSIST" left="{x}" top="{y}" width="{w}" height="{h}"/>'
        for name, (x, y, w, h) in PIECES.items()]


def preview(atlas):
    """The panel put together roughly as assist_panel.c draws it (no text), on a game-like grey."""
    p = Image.new("RGBA", (PANEL_W + 40, PANEL_H + 40), (58, 66, 52, 255))
    piece = lambda n: atlas.crop((PIECES[n][0], PIECES[n][1], PIECES[n][0] + PIECES[n][2], PIECES[n][1] + PIECES[n][3]))
    ox, oy = 20, 20
    p.alpha_composite(piece("back"), (ox, oy))
    p.alpha_composite(piece("close"), (ox + PANEL_W - BOX - 8, oy + 6))
    p.alpha_composite(piece("track"), (ox + 16, oy + 50))
    p.alpha_composite(piece("plate_full"), (ox + 16 + TRACK_W - PLATE_W - 3, oy + 53))
    for k, n in enumerate(("box_on", "box_on", "box_hover", "box_on", "box_off")):
        p.alpha_composite(piece(n), (ox + 20, oy + 152 + 28 * k))
    p.alpha_composite(piece("slider"), (ox + 24, oy + 326))
    p.alpha_composite(piece("knob"), (ox + 24 + 90 - KNOB // 2, oy + 326 + SLIDER_H // 2 - KNOB // 2))
    return p


def main(out):
    sys.path.insert(0, HERE)
    from build_menu_theme import write_tga
    atlas = build()
    write_tga(atlas, os.path.join(out, ATLAS))
    prev = preview(atlas)
    prev.resize((prev.width * 2, prev.height * 2), Image.LANCZOS).save(
        os.path.join(HERE, "..", "preview_assist_panel.png"))


if __name__ == "__main__":
    main(sys.argv[1])
