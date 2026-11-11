r"""The Survival reward cards' art (camera-smooth\src\cards.c), drawn here (no source picture) in the
theme's look: dark steel, gold rims, a coloured band per group.

    python build_survival_cards.py <out mod_override folder>

Output: art\revived\survival_cards.tga, one 1024 x 1024 atlas, declared in interface.xml by
build_menu_theme.py (tex_lines below; IFACEMAINBASE, where the DLL looks its textures up). The
sizes are cards.c's (CW x CH, PW x PH, ICON):

  card_build, card_hero, card_army, card_now   a card of each group (buildings steel blue, the
                     hero gold, the army red, at once green), CARD_W x CARD_H
  card_rare          a second gold rim with corner pieces, laid over a rare card
  card_plate         the panel behind the cards, PANEL_W x PANEL_H
  card_icon_<k>      the emblem of card kind k (cards.c's K_* order), ICON x ICON: gold on a dark
                     disc
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_assist_panel as A  # noqa: E402  (the drawing helpers and colours)

ATLAS = "art\\revived\\survival_cards.tga"
CARD_W, CARD_H = 200, 290
PANEL_W, PANEL_H = 3 * CARD_W + 4 * 22, 62 + CARD_H + 46
ICON = 76
GROUPS = {  # band colour (top, bottom)
    "build": ((0.42, 0.50, 0.60), (0.18, 0.23, 0.30)),
    "hero": ((0.86, 0.66, 0.20), (0.45, 0.30, 0.06)),
    "army": ((0.72, 0.22, 0.17), (0.34, 0.07, 0.05)),
    "now": ((0.30, 0.62, 0.32), (0.10, 0.30, 0.12)),
}
# the emblem of each kind, in cards.c's order
ICONS = ["wall", "arrow_up", "ball", "chevrons", "shield",
         "sword", "shield_star", "eye", "star", "heads", "drop",
         "swords", "shield", "chevrons", "cross", "arrow", "ranks",
         "sword", "horseshoe", "ball_arrow",
         "coins", "helmet", "hourglass"]

PIECES = {"card_" + g: (i * (CARD_W + 4), 0, CARD_W, CARD_H) for i, g in enumerate(GROUPS)}
PIECES["card_rare"] = (4 * (CARD_W + 4), 0, CARD_W, CARD_H)
PIECES["card_plate"] = (0, CARD_H + 6, PANEL_W, PANEL_H)
for k in range(len(ICONS)):
    PIECES["card_icon_%d" % k] = (PANEL_W + 6 + (k % 4) * (ICON + 4), CARD_H + 6 + (k // 4) * (ICON + 4), ICON, ICON)


def card(group):
    c = A.Canvas(CARD_W, CARD_H)
    W, H = CARD_W, CARD_H
    outer = A.rrect_sdf(c.x, c.y, 0.5, 0.5, W - 0.5, H - 0.5, 10)
    base = A.vgrad(c, (0.15, 0.165, 0.185), (0.07, 0.075, 0.088))
    base = np.clip(base + A.brushed(c, 7, 0.012)[..., None], 0, 1)
    c.paint(A.cover(outer), base, 0.97)
    vx = np.abs(c.x / W - 0.5) * 2
    vy = np.abs(c.y / H - 0.5) * 2
    c.paint(A.cover(outer) * np.clip(np.maximum(vx, vy) ** 3, 0, 1) * 0.4, (0, 0, 0))
    # the group's band at the top
    top, bottom = GROUPS[group]
    band = A.cover(A.rrect_sdf(c.x, c.y, 6, 6, W - 6, 34, 6))
    c.paint(band, A.vgrad(c, top, bottom, 6, 34), 0.95)
    c.paint(band * np.exp(-((c.y - 11) / 4) ** 2), (1, 1, 1), 0.10)
    # the emblem's well: a dark disc with a soft light round it
    cx, cy, r = W / 2, 40 + ICON / 2, ICON / 2 + 6
    halo = np.exp(-(np.maximum(A.circle_sdf(c.x, c.y, cx, cy, r), 0) / 14) ** 2)
    c.paint(halo * A.cover(outer), np.array(top), 0.18)
    well = A.circle_sdf(c.x, c.y, cx, cy, r)
    c.paint(A.cover(well), (0.03, 0.035, 0.04), 0.9)
    c.paint(A.cover(np.abs(well + 1) - 1), np.array(top) * 0.9, 0.9)
    # dividers above the effect text and the bottom line
    for y in (170, H - 40):
        line = A.cover(np.abs(c.y - y) - 0.5) * A.cover(np.abs(c.x - W / 2) - (W / 2 - 22))
        c.paint(line, A.GOLD, 0.45)
    # bevelled frame and a thin inner gold line
    frame = A.cover(outer) * A.cover(-(outer + 3))
    u = np.clip((c.x / W + c.y / H) / 2, 0, 1)[..., None]
    c.paint(frame, np.array([0.60, 0.62, 0.65]) * (1 - u) + np.array([0.12, 0.13, 0.15]) * u)
    c.paint(A.cover(outer) * A.cover(-(outer + 1)), (0, 0, 0), 0.8)
    inner = A.rrect_sdf(c.x, c.y, 6, 6, W - 6, H - 6, 6)
    c.paint(A.cover(np.abs(inner) - 0.5), A.GOLD, 0.5)
    for x, y in ((12, H - 12), (W - 12, H - 12)):
        A.rivet(c, x, y, 3.0)
    return c.image()


def rare():
    c = A.Canvas(CARD_W, CARD_H)
    W, H = CARD_W, CARD_H
    outer = A.rrect_sdf(c.x, c.y, 2, 2, W - 2, H - 2, 9)
    A.gold_ring(c, outer, 3.0)
    inner = A.rrect_sdf(c.x, c.y, 9, 9, W - 9, H - 9, 5)
    c.paint(A.cover(np.abs(inner) - 0.8), A.GOLD_HI, 0.8)
    for x, y in ((14, 14), (W - 14, 14), (14, H - 14), (W - 14, H - 14)):
        d = np.abs(c.x - x) + np.abs(c.y - y) - 7  # a gold diamond in each corner
        c.paint(A.cover(d), A.GOLD_HI)
        c.paint(A.cover(np.abs(d) - 0.5), (0.35, 0.22, 0.04), 0.8)
    return c.image()


def plate():
    c = A.Canvas(PANEL_W, PANEL_H)
    W, H = PANEL_W, PANEL_H
    outer = A.rrect_sdf(c.x, c.y, 0.5, 0.5, W - 0.5, H - 0.5, 9)
    base = A.vgrad(c, (0.10, 0.11, 0.125), (0.045, 0.05, 0.058))
    base = np.clip(base + A.brushed(c, 3, 0.010)[..., None], 0, 1)
    c.paint(A.cover(outer), base, 0.94)
    c.paint(A.cover(np.abs(c.y - 58.5) - 0.5) * A.cover(np.abs(c.x - W / 2) - (W / 2 - 16)), A.GOLD, 0.8)
    c.paint(A.cover(np.abs(c.y - 59.5) - 0.5) * A.cover(np.abs(c.x - W / 2) - (W / 2 - 16)), (0, 0, 0), 0.6)
    frame = A.cover(outer) * A.cover(-(outer + 3))
    u = np.clip((c.x / W + c.y / H) / 2, 0, 1)[..., None]
    c.paint(frame, np.array([0.55, 0.57, 0.60]) * (1 - u) + np.array([0.12, 0.13, 0.15]) * u)
    c.paint(A.cover(outer) * A.cover(-(outer + 1)), (0, 0, 0), 0.8)
    inner = A.rrect_sdf(c.x, c.y, 6, 6, W - 6, H - 6, 5)
    c.paint(A.cover(np.abs(inner) - 0.5), A.GOLD, 0.5)
    for x, y in ((14, 14), (W - 14, 14), (14, H - 14), (W - 14, H - 14)):
        A.rivet(c, x, y, 3.4)
    return c.image()


# ---- the emblems: drawn as a mask at 8 x, then gold with a dark edge -------------------------

S = 8
N = ICON * S


def P(*pts):
    return [(x * N, y * N) for x, y in pts]


def shape(name, d):
    """Draws the emblem `name` in white on the L image's draw `d`, in 0..1 coordinates."""
    W = lambda w: int(w * N)  # noqa: E731
    if name == "wall":
        d.rectangle(P((0.22, 0.42), (0.78, 0.76)), fill=255)
        for i in range(4):
            x = 0.22 + i * 0.16
            d.rectangle(P((x, 0.30), (x + 0.09, 0.43)), fill=255)
        d.rectangle(P((0.44, 0.58), (0.56, 0.76)), fill=0)
    elif name in ("arrow", "arrow_up"):
        if name == "arrow_up":
            d.polygon(P((0.5, 0.18), (0.70, 0.42), (0.56, 0.42), (0.56, 0.80), (0.44, 0.80), (0.44, 0.42), (0.30, 0.42)), fill=255)
        else:
            d.line(P((0.24, 0.76), (0.66, 0.34)), fill=255, width=W(0.07))
            d.polygon(P((0.78, 0.22), (0.72, 0.46), (0.54, 0.28)), fill=255)
            d.polygon(P((0.20, 0.80), (0.20, 0.64), (0.30, 0.70)), fill=255)
            d.polygon(P((0.20, 0.80), (0.36, 0.80), (0.30, 0.70)), fill=255)
    elif name == "ball":
        d.ellipse(P((0.38, 0.38), (0.74, 0.74)), fill=255)
        for k in range(3):
            y = 0.40 + k * 0.10
            d.line(P((0.18, y + 0.06), (0.34, y + 0.06)), fill=255, width=W(0.04))
    elif name == "ball_arrow":
        d.ellipse(P((0.20, 0.50), (0.48, 0.78)), fill=255)
        d.line(P((0.46, 0.52), (0.70, 0.28)), fill=255, width=W(0.06))
        d.polygon(P((0.80, 0.18), (0.76, 0.40), (0.58, 0.22)), fill=255)
    elif name == "chevrons":
        for k in range(2):
            x = 0.26 + k * 0.24
            d.polygon(P((x, 0.24), (x + 0.10, 0.24), (x + 0.30, 0.50), (x + 0.10, 0.76), (x, 0.76), (x + 0.20, 0.50)), fill=255)
    elif name in ("shield", "shield_star"):
        d.polygon(P((0.5, 0.18), (0.78, 0.28), (0.76, 0.52), (0.66, 0.70), (0.5, 0.82), (0.34, 0.70), (0.24, 0.52), (0.22, 0.28)), fill=255)
        d.polygon(P((0.5, 0.26), (0.70, 0.33), (0.68, 0.51), (0.60, 0.65), (0.5, 0.73)), fill=150)
        if name == "shield_star":
            star(d, 0.5, 0.48, 0.16, 0)
    elif name in ("sword", "swords"):
        def sword(a):
            c, s_ = np.cos(a), np.sin(a)
            rot = lambda x, y: (0.5 + (x - 0.5) * c - (y - 0.5) * s_, 0.5 + (x - 0.5) * s_ + (y - 0.5) * c)  # noqa: E731
            d.polygon(P(rot(0.5, 0.14), rot(0.555, 0.24), rot(0.555, 0.62), rot(0.445, 0.62), rot(0.445, 0.24)), fill=255)
            d.polygon(P(rot(0.32, 0.62), rot(0.68, 0.62), rot(0.68, 0.68), rot(0.32, 0.68)), fill=255)
            d.polygon(P(rot(0.47, 0.68), rot(0.53, 0.68), rot(0.53, 0.82), rot(0.47, 0.82)), fill=255)
            x, y = rot(0.5, 0.86)
            d.ellipse(P((x - 0.04, y - 0.04), (x + 0.04, y + 0.04)), fill=255)
        if name == "sword":
            sword(0.0)
        else:
            sword(-0.62)
            sword(0.62)
    elif name == "eye":
        d.ellipse(P((0.16, 0.34), (0.84, 0.66)), fill=255)
        d.ellipse(P((0.36, 0.36), (0.64, 0.64)), fill=0)
        d.ellipse(P((0.42, 0.42), (0.58, 0.58)), fill=255)
    elif name == "star":
        star(d, 0.5, 0.52, 0.32, 255)
    elif name == "heads":
        for x, y, r in ((0.30, 0.42, 0.09), (0.70, 0.42, 0.09), (0.5, 0.36, 0.11)):
            d.ellipse(P((x - r, y - r), (x + r, y + r)), fill=255)
            d.pieslice(P((x - r * 1.9, y + r * 1.2), (x + r * 1.9, y + r * 4.6)), 180, 360, fill=255)
    elif name == "drop":
        d.ellipse(P((0.30, 0.42), (0.70, 0.82)), fill=255)
        d.polygon(P((0.5, 0.14), (0.68, 0.54), (0.32, 0.54)), fill=255)
        d.ellipse(P((0.38, 0.56), (0.48, 0.70)), fill=120)
    elif name == "cross":
        d.rectangle(P((0.42, 0.20), (0.58, 0.80)), fill=255)
        d.rectangle(P((0.20, 0.42), (0.80, 0.58)), fill=255)
    elif name == "ranks":
        for k in range(3):
            y = 0.22 + k * 0.18
            d.polygon(P((0.22, y), (0.5, y + 0.16), (0.78, y), (0.78, y + 0.10), (0.5, y + 0.26), (0.22, y + 0.10)), fill=255)
    elif name == "horseshoe":
        d.arc(P((0.24, 0.20), (0.76, 0.76)), 160, 380, fill=255, width=W(0.12))
        d.rectangle(P((0.22, 0.48), (0.33, 0.80)), fill=255)
        d.rectangle(P((0.67, 0.48), (0.78, 0.80)), fill=255)
        for x, y in ((0.30, 0.34), (0.70, 0.34), (0.5, 0.24)):
            d.ellipse(P((x - 0.025, y - 0.025), (x + 0.025, y + 0.025)), fill=0)
    elif name == "coins":
        for k in range(4):
            y = 0.66 - k * 0.11
            d.ellipse(P((0.26, y - 0.07), (0.74, y + 0.07)), fill=255)
            d.ellipse(P((0.30, y - 0.05), (0.70, y + 0.03)), fill=170)
        d.ellipse(P((0.26, 0.26), (0.74, 0.40)), fill=255)
    elif name == "helmet":
        d.pieslice(P((0.22, 0.24), (0.78, 0.80)), 180, 360, fill=255)
        d.rectangle(P((0.22, 0.51), (0.78, 0.60)), fill=255)
        d.rectangle(P((0.44, 0.18), (0.56, 0.30)), fill=255)
        d.polygon(P((0.30, 0.60), (0.42, 0.60), (0.40, 0.78), (0.30, 0.74)), fill=255)
        d.polygon(P((0.70, 0.60), (0.58, 0.60), (0.60, 0.78), (0.70, 0.74)), fill=255)
    elif name == "hourglass":
        d.rectangle(P((0.26, 0.16), (0.74, 0.22)), fill=255)
        d.rectangle(P((0.26, 0.78), (0.74, 0.84)), fill=255)
        d.polygon(P((0.30, 0.22), (0.70, 0.22), (0.54, 0.50), (0.70, 0.78), (0.30, 0.78), (0.46, 0.50)), fill=255)
        d.polygon(P((0.38, 0.28), (0.62, 0.28), (0.5, 0.46)), fill=110)
        d.polygon(P((0.5, 0.60), (0.64, 0.74), (0.36, 0.74)), fill=170)
    else:
        raise ValueError(name)


def star(d, cx, cy, r, fill):
    pts = []
    for i in range(10):
        a = -np.pi / 2 + i * np.pi / 5
        rr = r if i % 2 == 0 else r * 0.45
        pts.append((cx + rr * np.cos(a), cy + rr * np.sin(a)))
    d.polygon(P(*pts), fill=fill if fill else 0)


def icon(name):
    m = Image.new("L", (N, N), 0)
    shape(name, ImageDraw.Draw(m))
    body = np.asarray(m, dtype=float) / 255
    solid = (body > 0).astype(float)
    edge = np.asarray(Image.fromarray((solid * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(2 * S + 1)), dtype=float) / 255
    yy = (np.arange(N) / N)[:, None]
    gold = A.GOLD_HI * (1 - yy[..., None]) + A.GOLD_LO * yy[..., None]
    shade = 0.55 + 0.45 * body  # the grey parts of the mask: darker gold
    rgb = gold * shade[..., None]
    rgb = np.where(solid[..., None] > 0, rgb, np.array([0.06, 0.04, 0.01]))
    a = np.maximum(solid, edge * 0.85)
    img = Image.fromarray((np.dstack([np.clip(rgb, 0, 1), a]) * 255 + 0.5).astype(np.uint8), "RGBA")
    return img.resize((ICON, ICON), Image.LANCZOS)


def build():
    pics = {"card_" + g: card(g) for g in GROUPS}
    pics["card_rare"] = rare()
    pics["card_plate"] = plate()
    for k, name in enumerate(ICONS):
        pics["card_icon_%d" % k] = icon(name)
    atlas = Image.new("RGBA", (1024, 1024))
    for name, (x, y, w, h) in PIECES.items():
        assert pics[name].size == (w, h), name
        assert x + w <= 1024 and y + h <= 1024, name
        atlas.paste(pics[name], (x, y))
    return atlas


def tex_lines():
    return ['<IFACETEXLOAD id="TEX_REVIVED_CARDS" file=".\\%s" pixformat="PF_32"/>' % ATLAS] + [
        f'<IFACETEX name="{name}" texture="TEX_REVIVED_CARDS" left="{x}" top="{y}" width="{w}" height="{h}"/>'
        for name, (x, y, w, h) in PIECES.items()]


def preview(atlas):
    """The boss choice roughly as cards.c draws it (no text), on a game-like green."""
    p = Image.new("RGBA", (PANEL_W + 40, PANEL_H + 40), (58, 66, 52, 255))
    piece = lambda n: atlas.crop((PIECES[n][0], PIECES[n][1], PIECES[n][0] + PIECES[n][2], PIECES[n][1] + PIECES[n][3]))  # noqa: E731
    p.alpha_composite(piece("card_plate"), (20, 20))
    for i, (g, k) in enumerate((("build", 0), ("hero", 9), ("army", 11))):
        x, y = 20 + 22 + i * (CARD_W + 22), 20 + 62
        p.alpha_composite(piece("card_" + g), (x, y))
        if k == 9:
            p.alpha_composite(piece("card_rare"), (x, y))
        p.alpha_composite(piece("card_icon_%d" % k), (x + (CARD_W - ICON) // 2, y + 40))
    sheet = Image.new("RGBA", (8 * (ICON + 8), 3 * (ICON + 8)), (40, 44, 40, 255))
    for k in range(len(ICONS)):
        sheet.alpha_composite(piece("card_icon_%d" % k), (4 + (k % 8) * (ICON + 8), 4 + (k // 8) * (ICON + 8)))
    return p, sheet


def main(out, write_tga=None):
    atlas = build()
    if write_tga is None:  # run by itself (build_menu_theme.py passes its own writer)
        os.makedirs(os.path.dirname(os.path.join(out, ATLAS)), exist_ok=True)
        atlas.save(os.path.join(out, ATLAS))
    else:
        write_tga(atlas, os.path.join(out, ATLAS))
    prev, sheet = preview(atlas)
    prev.save(os.path.join(HERE, "..", "preview_survival_cards.png"))
    sheet.resize((sheet.width * 2, sheet.height * 2), Image.LANCZOS).save(os.path.join(HERE, "..", "preview_card_icons.png"))


if __name__ == "__main__":
    main(sys.argv[1])
