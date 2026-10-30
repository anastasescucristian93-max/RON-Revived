r"""Cuts the nation flag medallions out of the Gemini sheets into one interface atlas.

    python build_flags.py <out mod_override folder>

Sources: source\flags\flags_1..5.jpg, each a grid of 3 columns of round medallions (gold rim)
on white. Each medallion is found by its bounding box, cut as a square, made transparent outside
the circle and scaled down to CELL x CELL pixels. A medallion cleaned by hand (background
removed from export_flag_squares.py's square, PNG with alpha) in source\flags\cut\<nation>.png
gives that medallion's edge; the average edge of the cleaned ones is used for the rest.

Output: art\revived\nation_flags.tga (8 x 4 cells). Slot = tribe index in rules.xml order,
so the DLL (camera-smooth\src\flags.c) finds the flag of tribe t as IFACETEX "flag_<t>";
build_menu_theme.py declares those names in setupwin.xml (tex_lines below). The last cell is
opaque white ("flag_bg"): nation_grid.c draws it in the card's colour under each flag.
"""
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "source", "flags")
CUT = os.path.join(SRC, "cut")
CELL = 64  # the nation grid draws them at 42 px (camera-smooth\src\nation_grid.c)
MARGIN = 1  # transparent pixels around each medallion, so filtering never reaches the next cell
MIN_ALPHA = 16  # fainter edge pixels are dropped (see shrink)
COLS, ROWS = 8, 4
BG_SLOT = COLS * ROWS - 1  # opaque white: the DLL draws it, tinted, under each flag
ATLAS = "art\\revived\\nation_flags.tga"

# Tribe keys in rules.xml order (the tribe index the setup screen uses as item id), then
# flags for nations the game does not have yet. Romania is tribe 24 with the romania mod
# (camera-smooth\src\romania.c); without it flag_24 is simply never asked for.
TRIBES = ["aztecs", "maya", "inca", "bantu", "nubians", "greeks", "romans", "egyptians", "turks",
          "spanish", "french", "british", "germans", "russians", "chinese", "japanese", "koreans",
          "mongols", "iroquois", "lakota", "americans", "indians", "dutch", "persian",
          "romania", "italy"]
NUM_TRIBES = 25

# Medallions per sheet, 3 per row, left to right, top row first; None skips one (flags_2 has
# a second Aztec sun stone, replaced by the Maya glyph of flags_5).
SHEETS = {
    "flags_1.jpg": ["aztecs", "bantu", "british", "chinese", "egyptians", "french"],
    "flags_2.jpg": ["germans", "greeks", None, "japanese", "mongols", "nubians"],
    "flags_3.jpg": ["romans", "romania", "russians", "spanish", "turks", "americans"],
    "flags_4.jpg": ["iroquois", "lakota", "dutch", "persian", "indians", "italy"],
    "flags_5.jpg": ["inca", "koreans", "maya"],
}


def slot_rect(i):
    return (i % COLS) * CELL, (i // COLS) * CELL, CELL, CELL


def find_medallion(cell):
    """Centre and radius of the medallion in a grid cell: the bounding box of the clearly
    non-white pixels (the soft drop shadow stays lighter than the threshold). The shadow
    falls below, so the radius comes from the width."""
    a = np.asarray(cell.convert("RGB")).astype(int)
    mask = a.min(axis=2) < 200
    ys, xs = np.nonzero(mask)
    x0, x1 = xs.min(), xs.max()
    y0 = ys.min()
    r = (x1 - x0 + 1) / 2
    return (x0 + x1 + 1) / 2, y0 + r, r


def cut(img, cx, cy, r, profile):
    """A medallion without a cleaned file: a square crop around the circle, its alpha from
    the cleaned medallions' average edge, the white of the background taken out of the
    edge colours, scaled to CELL."""
    big = CELL * 8
    side = r * 1.04
    sq = img.crop((round(cx - side), round(cy - side), round(cx + side), round(cy + side))).convert("RGB")
    sq = sq.resize((big, big), Image.LANCZOS)
    yy, xx = np.mgrid[0:big, 0:big]
    d = np.hypot(xx + 0.5 - big / 2, yy + 0.5 - big / 2) / (big / 2) * 1.04
    alpha = np.interp(d, *profile) if profile else np.clip((0.985 - d) * big / 2 + 0.5, 0, 1)
    rgb = np.asarray(sq).astype(float) / 255
    a = np.clip(alpha, 0.05, 1)[..., None]
    rgb = np.clip((rgb - (1 - a)) / a, 0, 1)  # seen = a * colour + (1 - a) * white
    return shrink(crop_square(np.dstack([rgb, alpha]), big / 2, big / 2, big / 2 / 1.04))


def circle_of(alpha):
    """Centre and radius of a cleaned medallion; the shadow left below is not counted."""
    cols = np.nonzero((alpha > 0.5).any(axis=0))[0]
    rows = np.nonzero((alpha > 0.5).any(axis=1))[0]
    r = (cols[-1] - cols[0] + 1) / 2
    return (cols[0] + cols[-1] + 1) / 2, rows[0] + r, r


def from_cleaned(path, img, cx, cy, r):
    """A hand-cleaned medallion (background removed). Outside the circle (the shadow) is
    cut away; inside it the colours come from the sheet with full alpha, because the
    cleaner also removed white parts of some flags (Japan, Russia)."""
    p = np.asarray(Image.open(path).convert("RGBA")).astype(float) / 255
    h, w = p.shape[:2]
    pcx, pcy, pr = circle_of(p[..., 3])
    k = r / pr  # sheet pixels per cleaned pixel
    sheet = img.crop((round(cx - pcx * k), round(cy - pcy * k), round(cx + (w - pcx) * k),
                      round(cy + (h - pcy) * k))).convert("RGB").resize((w, h), Image.LANCZOS)
    yy, xx = np.mgrid[0:h, 0:w]
    d = np.hypot(xx + 0.5 - pcx, yy + 0.5 - pcy)
    alpha = p[..., 3] * np.clip(pr + 0.5 - d, 0, 1)
    rgb = p[..., :3].copy()
    inside = d < pr * 0.88
    alpha[inside] = 1
    rgb[inside] = (np.asarray(sheet).astype(float) / 255)[inside]
    return np.dstack([rgb, alpha]), (pcx, pcy, pr)


def edge_profile(cleaned):
    """Average alpha by distance from the centre (in radii) over the top half of the
    cleaned medallions, where no shadow is left."""
    bins = np.linspace(0.80, 1.06, 53)
    sums, counts = np.zeros(len(bins)), np.zeros(len(bins))
    for rgba, (cx, cy, r) in cleaned:
        yy, xx = np.mgrid[0:rgba.shape[0], 0:rgba.shape[1]]
        top = yy + 0.5 < cy
        d = np.hypot(xx + 0.5 - cx, yy + 0.5 - cy)[top] / r
        i = np.clip(np.round((d - bins[0]) / (bins[1] - bins[0])).astype(int), 0, len(bins) - 1)
        np.add.at(sums, i, rgba[..., 3][top])
        np.add.at(counts, i, 1)
    prof = np.where(counts > 0, sums / np.maximum(counts, 1), 0)
    prof[bins < 0.85] = 1
    return bins, prof


def crop_square(rgba, cx, cy, r):
    """The square around a circle, padded with transparency where it leaves the image."""
    side = int(round(2 * r))
    out = np.zeros((side, side, 4))
    x0, y0 = int(round(cx - r)), int(round(cy - r))
    h, w = rgba.shape[:2]
    sx0, sy0, sx1, sy1 = max(x0, 0), max(y0, 0), min(x0 + side, w), min(y0 + side, h)
    out[sy0 - y0:sy1 - y0, sx0 - x0:sx1 - x0] = rgba[sy0:sy1, sx0:sx1]
    return out


def shrink(rgba):
    """Scales an RGBA float image to CELL - 2 * MARGIN with premultiplied alpha, so no
    background colour bleeds into the edge, centred in a CELL square. The game draws the
    flags in front of their card and even a nearly transparent pixel hides the card there
    (the screen behind shows through, a grey ring in game), so the faint ringing the
    scaling leaves around the circle is cut."""
    inner = CELL - 2 * MARGIN
    alpha = rgba[..., 3]
    pre = np.dstack([rgba[..., :3] * alpha[..., None], alpha])
    small = np.stack([np.asarray(Image.fromarray((pre[..., k] * 255).astype(np.uint8)).resize(
        (inner, inner), Image.LANCZOS)).astype(float) / 255 for k in range(4)], axis=2)
    a = np.clip(small[..., 3:], 1e-6, 1)
    out = np.dstack([np.clip(small[..., :3] / a, 0, 1), small[..., 3:]])
    out[out[..., 3] < MIN_ALPHA / 255] = 0
    cell = Image.new("RGBA", (CELL, CELL))
    cell.paste(Image.fromarray((out * 255 + 0.5).astype(np.uint8), "RGBA"), (MARGIN, MARGIN))
    return cell


def build():
    atlas = Image.new("RGBA", (COLS * CELL, ROWS * CELL))
    sheets = {}
    medallions = []  # (key, sheet, cx, cy, r)
    for name, keys in SHEETS.items():
        img = sheets[name] = Image.open(os.path.join(SRC, name))
        w, h = img.size
        rows = (len(keys) + 2) // 3
        for k, key in enumerate(keys):
            if key is None:
                continue
            col, row = k % 3, k // 3
            box = (col * w // 3, row * h // rows, (col + 1) * w // 3, (row + 1) * h // rows)
            cx, cy, r = find_medallion(img.crop(box))
            medallions.append((key, img, box[0] + cx, box[1] + cy, r))
    cleaned = {}
    for key, img, cx, cy, r in medallions:
        hand = os.path.join(CUT, key + ".png")
        if os.path.exists(hand):
            cleaned[key] = from_cleaned(hand, img, cx, cy, r)
    profile = edge_profile(cleaned.values()) if cleaned else None
    found, hand_made = set(), []
    for key, img, cx, cy, r in medallions:
        if key in cleaned:
            rgba, (pcx, pcy, pr) = cleaned[key]
            flag = shrink(crop_square(rgba, pcx, pcy, pr))
            hand_made.append(key)
        else:
            flag = cut(img, cx, cy, r, profile)
        x, y, _, _ = slot_rect(TRIBES.index(key))
        atlas.paste(flag, (x, y))
        found.add(key)
    x, y, _, _ = slot_rect(BG_SLOT)
    atlas.paste((255, 255, 255, 255), (x, y, x + CELL, y + CELL))
    print("cleaned by hand:", len(hand_made), "- cut here:", ", ".join(sorted(found - set(hand_made))))
    missing = [t for t in TRIBES if t not in found]
    return atlas, found, missing


def tex_lines(found):
    lines = []
    for i, key in enumerate(TRIBES):
        if key in found:
            x, y, w, h = slot_rect(i)
            # nations not in the game by key: ids 24 and up are the combo's other items
            name = i if i < NUM_TRIBES else key
            lines.append(f'<IFACETEX name="flag_{name}" texture="TEX_REVIVED_FLAGS" '
                         f'left="{x}" top="{y}" width="{w}" height="{h}"/>')
    # the white cell, inset so filtering stays inside it
    x, y, w, h = slot_rect(BG_SLOT)
    lines.append(f'<IFACETEX name="flag_bg" texture="TEX_REVIVED_FLAGS" '
                 f'left="{x + 8}" top="{y + 8}" width="{w - 16}" height="{h - 16}"/>')
    return lines


def main(out):
    sys.path.insert(0, HERE)
    from build_menu_theme import write_tga
    atlas, found, missing = build()
    write_tga(atlas, os.path.join(out, ATLAS))
    prev = Image.new("RGBA", atlas.size, (40, 44, 50, 255))
    prev.alpha_composite(atlas)
    prev.resize((atlas.width * 3, atlas.height * 3), Image.NEAREST).save(
        os.path.join(HERE, "..", "preview_flags.png"))
    print("missing flags:", ", ".join(missing))
    return found


if __name__ == "__main__":
    main(sys.argv[1])
