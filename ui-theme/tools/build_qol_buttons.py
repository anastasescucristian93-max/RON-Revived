r"""Cuts the round buttons of the qol row (camera-smooth\src\qol.c) out of the Gemini sheet
into one interface atlas, in three states.

    python build_qol_buttons.py <out mod_override folder>

Source: source\qol_buttons\buttons.jpg, four round medallions (black disc, gold rim, gold
symbol) on magenta, left to right in the order of the buttons: military buildings, airbases,
docks, AI assistant. Each disc is found by its bounding box, cut with a smooth circular
edge slightly inside the outline (the magenta fringe stays out) and scaled to CELL.

Output: art\revived\qol_buttons.tga, a column per button and a row per state (normal,
hover lighter, pressed darker); row MINIMAP_ROW holds the minimap size button, its three
states side by side. build_menu_theme.py declares them in interface.xml, in the
IFACEMAINBASE block, as IFACETEX "qol_<button>_<state>" (tex_lines below); panel.c draws
them in place of the buttons' type icons.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "source", "qol_buttons", "buttons.jpg")
CELL = 96  # drawn at 40 px times the interface scale (60 px at 1.5)
MARGIN = 1
MIN_ALPHA = 16
BUTTONS = 4
STATES = (("n", 1.0), ("h", 1.25), ("p", 0.72))  # name, brightness
ATLAS = "art\\revived\\qol_buttons.tga"
INSET = 1.5  # source pixels cut off the disc's edge
MINIMAP_ROW = 3  # the minimap size button (minimap.c): one cell per state, left to right
SKULL = os.path.join(HERE, "..", "source", "kills_skull.png")  # the army's kills (kills.c), any size
SKULL_CELL = (3, MINIMAP_ROW)  # its cell, beside the minimap button
SKULL_SMALL = (4 * CELL, MINIMAP_ROW * CELL, 24)  # x, y, size: the same skull for 14-16 unit spots


def find_discs(a):
    """(cx, cy, rx, ry) of each disc, left to right. Gemini drew them slightly oval (194 to
    211 px wide, 204 high), so each one is measured on its middle row and column and
    stretched back to a circle."""
    mag = (a[..., 0] > 180) & (a[..., 1] < 90) & (a[..., 2] > 180)
    solid = ~mag
    cols = np.nonzero(solid.sum(axis=0) > 3)[0]
    runs, start = [], cols[0]
    for p, c in zip(cols, cols[1:]):
        if c != p + 1:
            runs.append((start, p))
            start = c
    runs.append((start, cols[-1]))
    discs = []
    for x0, x1 in runs:
        part = solid[:, x0:x1 + 1]
        ys = np.nonzero(part[:, part.shape[1] // 2])[0]
        cy, ry = (ys[0] + ys[-1] + 1) / 2, (ys[-1] - ys[0] + 1) / 2
        xs = np.nonzero(part[int(cy)])[0]
        discs.append((x0 + (xs[0] + xs[-1] + 1) / 2, cy, (xs[-1] - xs[0] + 1) / 2, ry))
    assert len(discs) == BUTTONS, discs
    return discs


def cut(img, cx, cy, rx, ry):
    big = CELL * 8
    sq = img.crop((round(cx - rx), round(cy - ry), round(cx + rx), round(cy + ry))).convert("RGB")
    sq = sq.resize((big, big), Image.LANCZOS)
    yy, xx = np.mgrid[0:big, 0:big]
    d = np.hypot(xx + 0.5 - big / 2, yy + 0.5 - big / 2) * (ry / (big / 2))  # in source pixels
    alpha = np.clip(ry - INSET - d + 0.5, 0, 1)
    rgb = np.asarray(sq).astype(float) / 255
    return np.dstack([rgb, alpha])


def shrink(rgba, brightness):
    """To CELL - 2 * MARGIN with premultiplied alpha, centred in a CELL square; faint edge
    pixels are dropped (see build_flags.shrink)."""
    inner = CELL - 2 * MARGIN
    alpha = rgba[..., 3]
    pre = np.dstack([rgba[..., :3] * alpha[..., None], alpha])
    small = np.stack([np.asarray(Image.fromarray((pre[..., k] * 255).astype(np.uint8)).resize(
        (inner, inner), Image.LANCZOS)).astype(float) / 255 for k in range(4)], axis=2)
    a = np.clip(small[..., 3:], 1e-6, 1)
    out = np.dstack([np.clip(small[..., :3] / a * brightness, 0, 1), small[..., 3:]])
    out[out[..., 3] < MIN_ALPHA / 255] = 0
    cell = Image.new("RGBA", (CELL, CELL))
    cell.paste(Image.fromarray((out * 255 + 0.5).astype(np.uint8), "RGBA"), (MARGIN, MARGIN))
    return cell


def minimap_button(img, disc):
    """The minimap size button: the first medallion's gold rim around a plain black disc with
    a gold double arrow on the diagonal, drawn here (the Gemini sheet has no such symbol)."""
    rgba = cut(img, *disc)
    big = rgba.shape[0]
    yy, xx = np.mgrid[0:big, 0:big]
    r = np.hypot(xx + 0.5 - big / 2, yy + 0.5 - big / 2) / (big / 2)
    rgba[r < 0.86, :3] = 0.01
    glyph = Image.new("L", (big, big))
    d = ImageDraw.Draw(glyph)
    p = lambda x, y: (x * big, y * big)
    d.line([p(0.37, 0.63), p(0.63, 0.37)], fill=255, width=round(0.09 * big))
    d.polygon([p(0.73, 0.27), p(0.47, 0.27), p(0.73, 0.53)], fill=255)  # top right head
    d.polygon([p(0.27, 0.73), p(0.53, 0.73), p(0.27, 0.47)], fill=255)  # bottom left head
    a = np.asarray(glyph).astype(float)[..., None] / 255
    t = (yy / big)[..., None]
    gold = (np.array([250, 214, 110]) * (1 - t) + np.array([205, 148, 44]) * t) / 255
    rgba[..., :3] = rgba[..., :3] * (1 - a) + gold * a
    return rgba


def build():
    img = Image.open(SRC)
    discs = find_discs(np.asarray(img.convert("RGB")).astype(int))
    atlas = Image.new("RGBA", (512, 512))
    for i, disc in enumerate(discs):
        rgba = cut(img, *disc)
        for s, (_, k) in enumerate(STATES):
            atlas.paste(shrink(rgba, k), (i * CELL, s * CELL))
    rgba = minimap_button(img, discs[0])
    for s, (_, k) in enumerate(STATES):
        atlas.paste(shrink(rgba, k), (s * CELL, MINIMAP_ROW * CELL))
    if os.path.exists(SKULL):  # square, cut out (source\kills_skull_gemini.jpg, the user's, 2026-11-01)
        skull = np.asarray(Image.open(SKULL).convert("RGBA")).astype(float) / 255
        atlas.paste(shrink(skull, 1.0), (SKULL_CELL[0] * CELL, SKULL_CELL[1] * CELL))
        # a small copy for the 14-16 unit spots, premultiplied for the downscale as in shrink()
        pre = np.dstack([skull[..., :3] * skull[..., 3:], skull[..., 3:]])
        k = SKULL_SMALL[2]
        sm = np.stack([np.asarray(Image.fromarray((pre[..., c] * 255).astype(np.uint8)).resize((k, k), Image.LANCZOS))
                       .astype(float) / 255 for c in range(4)], axis=2)
        al = np.clip(sm[..., 3:], 1e-6, 1)
        sm = np.dstack([np.clip(sm[..., :3] / al, 0, 1), sm[..., 3:]])
        atlas.paste(Image.fromarray((sm * 255 + 0.5).astype(np.uint8), "RGBA"), SKULL_SMALL[:2])
    return atlas


def tex_lines():
    return [f'<IFACETEX name="qol_{i}_{name}" texture="TEX_REVIVED_QOL" '
            f'left="{i * CELL}" top="{s * CELL}" width="{CELL}" height="{CELL}"/>'
            for i in range(BUTTONS) for s, (name, _) in enumerate(STATES)] + [
        f'<IFACETEX name="minimap_size_{name}" texture="TEX_REVIVED_QOL" '
        f'left="{s * CELL}" top="{MINIMAP_ROW * CELL}" width="{CELL}" height="{CELL}"/>'
        for s, (name, _) in enumerate(STATES)] + [
        f'<IFACETEX name="kills_skull" texture="TEX_REVIVED_QOL" left="{SKULL_CELL[0] * CELL}" '
        f'top="{SKULL_CELL[1] * CELL}" width="{CELL}" height="{CELL}"/>',
        f'<IFACETEX name="kills_skull_small" texture="TEX_REVIVED_QOL" left="{SKULL_SMALL[0]}" '
        f'top="{SKULL_SMALL[1]}" width="{SKULL_SMALL[2]}" height="{SKULL_SMALL[2]}"/>']


def main(out):
    sys.path.insert(0, HERE)
    from build_menu_theme import write_tga
    atlas = build()
    write_tga(atlas, os.path.join(out, ATLAS))
    prev = Image.new("RGBA", (BUTTONS * CELL, (MINIMAP_ROW + 1) * CELL), (40, 44, 50, 255))  # skull: column 3
    prev.alpha_composite(atlas.crop((0, 0, BUTTONS * CELL, (MINIMAP_ROW + 1) * CELL)))
    prev.resize((prev.width * 2, prev.height * 2), Image.NEAREST).save(
        os.path.join(HERE, "..", "preview_qol_buttons.png"))


if __name__ == "__main__":
    main(sys.argv[1])
