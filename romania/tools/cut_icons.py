r"""Cuts a Gemini sheet of unit icons (painted rectangles in a grid on magenta) into icons\.

    python cut_icons.py <sheet image> <unit NAME[@fix...]> [...]

A fix after a name corrects one portrait that does not match its sheet: @soft (less colour, a
little blur, warmer), @bronze (black metal to bronze). Example: Cannon@bronze@soft.

The rectangles are found as the non-magenta blocks of the sheet and taken row by row, left to
right; each one is saved as icons\<unit name in lower case, _ for the rest>.png, the name that
build_data.py looks for. There must be exactly as many rectangles as names. build_data.py then
scales them to the game's 61 x 39 (and 30 x 19) cells.

Gemini paints the figures smaller and paler than the game's icons (sheet 1, 2026-10-26): each
rectangle is cut closer (ZOOM, more from the bottom than the top, to keep the heads) and given
more colour and contrast, which made them read like the game's at 61 x 39.
"""
import os
import re
import sys

import numpy as np
from PIL import Image, ImageEnhance, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ICONS = os.path.join(HERE, "..", "icons")
ZOOM = 1.3
TOP_SHARE = 0.35  # of the rows cut away, the share taken from the top
COLOR, CONTRAST = 1.35, 1.25


def bronze(img):
    """Dark grey pixels (a cannon painted black) turned to bronze."""
    rgb = np.asarray(img).astype(float)
    hsv = np.asarray(img.convert("HSV")).astype(float) / 255
    w = np.clip((0.55 - hsv[..., 2]) / 0.4, 0, 1) * np.clip((0.45 - hsv[..., 1]) / 0.3, 0, 1) * 0.9
    target = np.array([150, 105, 55], float) * (0.35 + rgb.mean(axis=2, keepdims=True) / 255 * 1.3)
    return Image.fromarray(np.clip(rgb * (1 - w[..., None]) + target * w[..., None], 0, 255).astype(np.uint8))


def soft(img):
    """For a portrait painted harder and brighter than the rest of its sheet: less colour, a
    little blur, warmer (sheet 3's cannon)."""
    img = ImageEnhance.Contrast(ImageEnhance.Color(img).enhance(0.8)).enhance(0.95).filter(ImageFilter.GaussianBlur(0.8))
    arr = np.asarray(img).astype(float)
    arr = arr * 0.4 + arr * np.array([1.08, 1.0, 0.86]) * 0.6
    return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))


FIXES = {"bronze": bronze, "soft": soft}  # name@fix[@fix]; "soft" also leaves out the colour boost


def tune(img, fixes=()):
    for f in fixes:
        img = FIXES[f](img)
    w, h = img.size
    # a cell squarer than the game's 61:39 loses its top and bottom when build_data.py fits it,
    # so it is cut less here (sheet 5 came with nearly square cells: heads and rockets were lost)
    zoom = max(1.0, ZOOM * (w / h) / (61 / 39))
    cw, ch = w / zoom, h / zoom
    x0, y0 = (w - cw) / 2, (h - ch) * TOP_SHARE
    img = img.crop((round(x0), round(y0), round(x0 + cw), round(y0 + ch)))
    if "soft" in fixes:
        return img
    return ImageEnhance.Contrast(ImageEnhance.Color(img).enhance(COLOR)).enhance(CONTRAST)


def slug(name):
    return re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")


def runs(mask, min_len):
    """(start, end) of the runs of True at least min_len long."""
    out, start = [], None
    for i, v in enumerate(list(mask) + [False]):
        if v and start is None:
            start = i
        elif not v and start is not None:
            if i - start >= min_len:
                out.append((start, i))
            start = None
    return out


def main(sheet, names):
    global COLOR, CONTRAST
    if names and names[0].startswith("--color="):  # a sheet already richer than the game's icons
        COLOR = CONTRAST = float(names.pop(0).split("=")[1])
    img = Image.open(sheet).convert("RGB")
    a = np.asarray(img).astype(int)
    magenta = (a[..., 0] > 180) & (a[..., 1] < 90) & (a[..., 2] > 180)
    art = ~magenta
    min_w, min_h = img.width // 20, img.height // 20
    boxes = []
    for y0, y1 in runs(art.mean(axis=1) > 0.05, min_h):
        band = art[y0:y1]
        for x0, x1 in runs(band.mean(axis=0) > 0.3, min_w):
            # trim the block to its own rows (bands can join blocks of slightly different height)
            rows = np.where(art[y0:y1, x0:x1].mean(axis=1) > 0.3)[0]
            boxes.append((x0, y0 + rows[0], x1, y0 + rows[-1] + 1))
    if len(boxes) != len(names):
        raise SystemExit("%d blocks found, %d names given: %s" % (len(boxes), len(names), boxes))
    os.makedirs(ICONS, exist_ok=True)
    for (x0, y0, x1, y1), arg in zip(boxes, names):
        name, *fixes = arg.split("@")
        inset = 2  # leave out the anti-aliased edge against the magenta
        icon = tune(img.crop((x0 + inset, y0 + inset, x1 - inset, y1 - inset)), fixes)
        path = os.path.join(ICONS, slug(name) + ".png")
        icon.save(path)
        print("%-24s %4dx%-4d %s" % (name, icon.width, icon.height, os.path.relpath(path, os.path.join(HERE, ".."))))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2:])
