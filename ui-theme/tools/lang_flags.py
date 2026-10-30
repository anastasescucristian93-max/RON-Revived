r"""The launcher's language button: a round flag per language and the frame laid over each
(camera-smooth\src\launcher_lang.c). Called by build_launcher.py.

Inputs, when present: source\launcher\flags\<code>.png (a flag, any shape: it is fitted to a
square and cut round) and source\launcher\flags\frame.png (one ring on flat magenta, as Gemini
makes them; frames_sheet.jpg is the sheet it was cut from). Missing ones are drawn here as
placeholders.

Outputs: <res>\lang\<code>.png and <res>\lang\frame.png, SIZE x SIZE with alpha.
"""
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageOps

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "..", "source", "launcher", "flags")

CODES = ["DE", "EN", "ES", "FR", "IT", "RU", "PT", "PL", "TR", "RO"]  # the DLL's (the game's) order
SIZE = 128        # the DLL scales them to the button (60 px at 100 %) and the petals (46 px)
FLAG_R = 0.355    # the flag's radius, of SIZE: under the frame's inner edge
RING_IN = 0.34    # the frame's inner and outer radius, of SIZE
RING_OUT = 0.50
BIG = 512         # drawn at this size, then reduced
BUTTON, PETAL, RING = 64, 50, 90  # as launcher_lang.c draws them at 100 %: keep the two the same
GAP = 14


def stripes(colours, vertical=False):
    img = Image.new("RGB", (BIG, BIG))
    d = ImageDraw.Draw(img)
    total = sum(w for _, w in colours)
    at = 0
    for c, w in colours:
        a, b = round(at * BIG / total), round((at + w) * BIG / total)
        d.rectangle((a, 0, b, BIG) if vertical else (0, a, BIG, b), fill=c)
        at += w
    return img


def union_jack():
    img = Image.new("RGB", (BIG, BIG), (1, 33, 105))
    d = ImageDraw.Draw(img)
    w = BIG
    d.line((0, 0, w, w), fill="white", width=w // 6)
    d.line((0, w, w, 0), fill="white", width=w // 6)
    d.line((0, 0, w, w), fill=(200, 16, 46), width=w // 18)
    d.line((0, w, w, 0), fill=(200, 16, 46), width=w // 18)
    d.rectangle((w * 0.38, 0, w * 0.62, w), fill="white")
    d.rectangle((0, w * 0.38, w, w * 0.62), fill="white")
    d.rectangle((w * 0.43, 0, w * 0.57, w), fill=(200, 16, 46))
    d.rectangle((0, w * 0.43, w, w * 0.57), fill=(200, 16, 46))
    return img


def brazil():
    img = Image.new("RGB", (BIG, BIG), (0, 156, 59))
    d = ImageDraw.Draw(img)
    c, r = BIG / 2, BIG * 0.46
    d.polygon([(c, c - r * 0.78), (c + r, c), (c, c + r * 0.78), (c - r, c)], fill=(255, 223, 0))
    d.ellipse((c - BIG * 0.21, c - BIG * 0.21, c + BIG * 0.21, c + BIG * 0.21), fill=(0, 39, 118))
    band = Image.new("L", (BIG, BIG), 0)
    ImageDraw.Draw(band).arc((c - BIG * 0.42, c - BIG * 0.07, c + BIG * 0.30, c + BIG * 0.62), 205, 300,
                             fill=255, width=BIG // 30)
    disc = Image.new("L", (BIG, BIG), 0)
    ImageDraw.Draw(disc).ellipse((c - BIG * 0.21, c - BIG * 0.21, c + BIG * 0.21, c + BIG * 0.21), fill=255)
    img.paste((255, 255, 255), mask=Image.fromarray(np.minimum(np.asarray(band), np.asarray(disc))))
    return img


def turkey():
    img = Image.new("RGB", (BIG, BIG), (227, 10, 23))
    d = ImageDraw.Draw(img)
    c = BIG / 2
    r1, r2 = BIG * 0.25, BIG * 0.2
    x1, x2 = c - BIG * 0.08, c - BIG * 0.02
    d.ellipse((x1 - r1, c - r1, x1 + r1, c + r1), fill="white")
    d.ellipse((x2 - r2, c - r2, x2 + r2, c + r2), fill=(227, 10, 23))
    sx, sr = c + BIG * 0.22, BIG * 0.1
    pts = [(sx + (sr if i % 2 == 0 else sr * 0.4) * math.cos(math.pi * i / 5 + math.pi),
            c + (sr if i % 2 == 0 else sr * 0.4) * math.sin(math.pi * i / 5 + math.pi)) for i in range(10)]
    d.polygon(pts, fill="white")
    return img


def placeholder(code):
    return {
        "DE": lambda: stripes([((0, 0, 0), 1), ((221, 0, 0), 1), ((255, 206, 0), 1)]),
        "EN": union_jack,
        "ES": lambda: stripes([((170, 21, 27), 1), ((241, 191, 0), 2), ((170, 21, 27), 1)]),
        "FR": lambda: stripes([((0, 35, 149), 1), ((255, 255, 255), 1), ((237, 41, 57), 1)], True),
        "IT": lambda: stripes([((0, 146, 70), 1), ((255, 255, 255), 1), ((206, 43, 55), 1)], True),
        "RU": lambda: stripes([((255, 255, 255), 1), ((0, 57, 166), 1), ((213, 43, 30), 1)]),
        "PT": brazil,
        "PL": lambda: stripes([((255, 255, 255), 1), ((220, 20, 60), 1)]),
        "TR": turkey,
        "RO": lambda: stripes([((0, 43, 127), 1), ((252, 209, 22), 1), ((206, 17, 38), 1)], True),
    }[code]()


def radius_map(n):
    y, x = np.mgrid[0:n, 0:n] + 0.5
    return np.hypot(x - n / 2, y - n / 2) / n, x / n, y / n


def disc_mask(n, r):
    d, _, _ = radius_map(n)
    return np.clip((r - d) * n + 0.5, 0, 1)


def flag(code):
    p = os.path.join(SOURCE, f"{code}.png")
    if os.path.exists(p):
        img = ImageOps.fit(Image.open(p).convert("RGB"), (BIG, BIG), Image.LANCZOS)
    else:
        img = placeholder(code)
    # The whole flag inside the frame's opening (only its corners are cut), over itself at full
    # size for what the frame's inner edge hides.
    side = round(2 * RING_IN * BIG)
    img.paste(img.resize((side, side), Image.LANCZOS), ((BIG - side) // 2, (BIG - side) // 2))
    rgb = np.asarray(img, dtype=float)
    # Light from the upper right (as the game's), a soft shade toward the lower left.
    d, x, y = radius_map(BIG)
    light = 1.0 + 0.16 * ((x - 0.5) - (y - 0.5)) - 0.35 * np.clip((d - 0.30) / 0.14, 0, 1) ** 2
    rgb = np.clip(rgb * light[..., None], 0, 255)
    a = disc_mask(BIG, FLAG_R) * 255
    out = Image.fromarray(np.dstack([rgb, a]).astype(np.uint8), "RGBA")
    return out.resize((SIZE, SIZE), Image.LANCZOS)


def bilinear(a, sx, sy):
    h, w = a.shape[:2]
    sx = np.clip(sx - 0.5, 0, w - 1.001)
    sy = np.clip(sy - 0.5, 0, h - 1.001)
    x0, y0 = sx.astype(int), sy.astype(int)
    fx, fy = (sx - x0)[..., None], (sy - y0)[..., None]
    return (a[y0, x0] * (1 - fx) * (1 - fy) + a[y0, x0 + 1] * fx * (1 - fy) +
            a[y0 + 1, x0] * (1 - fx) * fy + a[y0 + 1, x0 + 1] * fx * fy)


def keyed_ring(img):
    """A ring on magenta: its centre and radii from the metal (not magenta), then cut by an exact
    ring (no magenta fringe, no drop shadow), its width pulled in to RING_IN..RING_OUT so the
    flags keep their size, mirrored so its light comes from the upper right as the game's."""
    a = np.asarray(img.convert("RGB"), dtype=float)
    metal = (a[..., 0] + a[..., 2]) / 2 - a[..., 1] < 45
    ys, xs = np.nonzero(metal)
    cx, cy = (xs.min() + xs.max() + 1) / 2, (ys.min() + ys.max() + 1) / 2
    r = np.hypot(xs + 0.5 - cx, ys + 0.5 - cy)
    r_in, r_out = np.percentile(r, 0.5) + 1.5, np.percentile(r, 99.7) - 1.5
    n = BIG
    d, x, y = radius_map(n)
    lo, hi = RING_IN, RING_OUT - 0.004
    src_r = r_in + np.clip((d - lo) / (hi - lo), 0, 1) * (r_out - r_in)
    ang = np.arctan2(y - 0.5, x - 0.5)
    rgb = bilinear(a, cx - src_r * np.cos(ang), cy + src_r * np.sin(ang))
    alpha = np.clip((d - lo) * n + 0.5, 0, 1) * np.clip((hi - d) * n + 0.5, 0, 1)
    out = Image.fromarray(np.dstack([np.clip(rgb, 0, 255), alpha * 255]).astype(np.uint8), "RGBA")
    return out.resize((SIZE, SIZE), Image.LANCZOS)


def frame():
    p = os.path.join(SOURCE, "frame.png")
    if os.path.exists(p):
        return keyed_ring(Image.open(p))
    # Placeholder: a riveted steel ring like the launcher's plates, lit from the upper right.
    n = BIG
    d, x, y = radius_map(n)
    rng = np.random.default_rng(5)
    grain = np.asarray(Image.fromarray((rng.random((n // 4, n // 4)) * 255).astype(np.uint8))
                       .resize((n, n), Image.BICUBIC), dtype=float) / 255
    mid = (RING_IN + RING_OUT) / 2
    t = (d - RING_IN) / (RING_OUT - RING_IN)                     # 0 inner edge .. 1 outer edge
    bevel = np.cos(np.clip(t, 0, 1) * math.pi)                   # +1 inner, -1 outer
    side = ((x - 0.5) - (y - 0.5)) / np.maximum(d, 1e-6) / 1.42  # +1 upper right
    shade = 0.55 + 0.25 * side * -bevel + 0.08 * (grain - 0.5) + 0.12 * np.exp(-((d - mid) * 40) ** 2)
    base = np.array([112, 116, 120], dtype=float)
    rgb = base * shade[..., None]
    # rust specks
    rust = np.asarray(Image.fromarray((rng.random((n // 16, n // 16)) * 255).astype(np.uint8))
                      .resize((n, n), Image.BICUBIC), dtype=float) / 255
    k = np.clip((rust - 0.72) * 5, 0, 1)[..., None] * 0.55
    rgb = rgb * (1 - k) + np.array([120, 62, 30]) * shade[..., None] * k
    # dark lips at both edges
    lip = np.exp(-(t * 9) ** 2) + np.exp(-((1 - t) * 9) ** 2)
    rgb *= (1 - 0.55 * lip)[..., None]
    alpha = np.clip((d - RING_IN) * n + 0.5, 0, 1) * np.clip((RING_OUT - 0.008 - d) * n + 0.5, 0, 1)
    img = Image.fromarray(np.dstack([np.clip(rgb, 0, 255), alpha * 255]).astype(np.uint8), "RGBA")
    # rivets
    dr = ImageDraw.Draw(img)
    rr = n * 0.022
    for i in range(8):
        a = math.pi / 8 + i * math.pi / 4
        cx, cy = n / 2 + mid * n * math.cos(a), n / 2 + mid * n * math.sin(a)
        dr.ellipse((cx - rr * 1.25, cy - rr * 1.05, cx + rr * 1.05, cy + rr * 1.25), fill=(30, 30, 32, 255))
        dr.ellipse((cx - rr, cy - rr, cx + rr, cy + rr), fill=(150, 152, 155, 255))
        dr.ellipse((cx - rr * 0.1, cy - rr * 0.7, cx + rr * 0.6, cy - rr * 0.0), fill=(205, 207, 210, 255))
    return img.filter(ImageFilter.GaussianBlur(0.6)).resize((SIZE, SIZE), Image.LANCZOS)


def build(res):
    out = os.path.join(res, "lang")
    os.makedirs(out, exist_ok=True)
    for code in CODES:
        flag(code).save(os.path.join(out, f"{code}.png"))
    frame().save(os.path.join(out, "frame.png"))
    return out


def preview(bg, centre, scale, open_codes, current):
    """The button open, as the DLL draws it at the end of its opening, over `bg` (RGBA)."""
    res = Image.new("RGBA", bg.size)
    fr = frame()

    def put(code, cx, cy, size):
        f = flag(code).resize((size, size), Image.LANCZOS)
        f.alpha_composite(fr.resize((size, size), Image.LANCZOS))
        res.alpha_composite(f, (round(cx - size / 2), round(cy - size / 2)))

    cx, cy = centre
    shade_r = (RING + PETAL * 0.8) * scale
    d, _, _ = radius_map(round(shade_r * 2))
    a = 0.55 * (1 - np.clip((d * 2 - 0.55) / 0.45, 0, 1) ** 2 * (3 - 2 * np.clip((d * 2 - 0.55) / 0.45, 0, 1)))
    shade = Image.fromarray(np.dstack([np.zeros_like(a)] * 3 + [a * 255]).astype(np.uint8), "RGBA")
    res.alpha_composite(shade, (round(cx - shade_r), round(cy - shade_r)))
    m = len(open_codes)
    for j, code in enumerate(open_codes):
        ang = -math.pi / 2 + j * 2 * math.pi / m
        put(code, cx + RING * scale * math.cos(ang), cy + RING * scale * math.sin(ang), round(PETAL * scale))
    put(current, cx, cy, round(BUTTON * scale))
    out = bg.copy()
    out.alpha_composite(res)
    return out
