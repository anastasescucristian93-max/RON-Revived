r"""Cuts the round buttons of the qol row (camera-smooth\src\qol.c) out of the Gemini sheet
into one interface atlas, in three states.

    python build_qol_buttons.py <out mod_override folder>

Source: source\qol_buttons\buttons.jpg, four round medallions (black disc, gold rim, gold
symbol) on magenta, left to right in the order of the buttons: military buildings, airbases,
docks, AI assistant. Each disc is found by its bounding box, cut with a smooth circular
edge slightly inside the outline (the magenta fringe stays out) and scaled to CELL.

Output: art\revived\qol_buttons.tga, a column per button and a row per state (normal,
hover lighter, pressed darker). build_menu_theme.py declares them in interface.xml, in the
IFACEMAINBASE block, as IFACETEX "qol_<button>_<state>" (tex_lines below); panel.c draws
them in place of the buttons' type icons.
"""
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "source", "qol_buttons", "buttons.jpg")
CELL = 96  # drawn at 40 px times the interface scale (60 px at 1.5)
MARGIN = 1
MIN_ALPHA = 16
BUTTONS = 4
STATES = (("n", 1.0), ("h", 1.25), ("p", 0.72))  # name, brightness
ATLAS = "art\\revived\\qol_buttons.tga"
INSET = 1.5  # source pixels cut off the disc's edge


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


def build():
    img = Image.open(SRC)
    discs = find_discs(np.asarray(img.convert("RGB")).astype(int))
    atlas = Image.new("RGBA", (512, 512))
    for i, disc in enumerate(discs):
        rgba = cut(img, *disc)
        for s, (_, k) in enumerate(STATES):
            atlas.paste(shrink(rgba, k), (i * CELL, s * CELL))
    return atlas


def tex_lines():
    return [f'<IFACETEX name="qol_{i}_{name}" texture="TEX_REVIVED_QOL" '
            f'left="{i * CELL}" top="{s * CELL}" width="{CELL}" height="{CELL}"/>'
            for i in range(BUTTONS) for s, (name, _) in enumerate(STATES)]


def main(out):
    sys.path.insert(0, HERE)
    from build_menu_theme import write_tga
    atlas = build()
    write_tga(atlas, os.path.join(out, ATLAS))
    prev = Image.new("RGBA", (BUTTONS * CELL, len(STATES) * CELL), (40, 44, 50, 255))
    prev.alpha_composite(atlas.crop((0, 0, BUTTONS * CELL, len(STATES) * CELL)))
    prev.resize((prev.width * 2, prev.height * 2), Image.NEAREST).save(
        os.path.join(HERE, "..", "preview_qol_buttons.png"))


if __name__ == "__main__":
    main(sys.argv[1])
