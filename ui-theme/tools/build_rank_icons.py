r"""The 10 veteran rank insignia (camera-smooth\src\ranks.c) from the Gemini sheet, into one
interface atlas.

    python build_rank_icons.py <out mod_override folder>

Source: source\rank_icons\ranks.jpg, 3 x 3 shields on magenta: bronze, silver, gold rows, 1-3
chevrons. Gemini drew the chevrons pointing different ways and two in the first gold one, so
only the bronze row is used for the shapes: its chevrons are recoloured to silver and gold with
the colours of the sheet's own silver and gold chevrons (luminance quantiles mapped onto each
other).

Output: art\revived\rank_icons.tga, levels 1-9 left to right in rows of 3, CELL_W x CELL_H
each; level 10 (no source picture: level 9 with its grey rim turned gold and a gold star over the
chevrons) under the glow, at RANK10_X, RANK10_Y. build_menu_theme.py declares them in interface.xml, in the IFACEMAINBASE block, as
IFACETEX "rank_<level>" (tex_lines below); ranks.c draws them on the selected unit's portrait.

Beside them, drawn here (no source picture): "rank_glow", a soft white glow with faint rays, and
"rank_spark", a small four-pointed star. rankfx.c tints both with the tier's colour for the
promotion effect. And "heal_plus", a rounded white cross with a dark rim: heal.c tints it green
and floats it over the units a General's Heal is healing. And "repair_pliers", steel pliers with red
grips in a dark rim, drawn in their own colours: cityrepair.c floats them over a building a city
is repairing.
"""
import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "source", "rank_icons", "ranks.jpg")
ATLAS = "art\\revived\\rank_icons.tga"
CELL_W, CELL_H = 48, 56  # drawn about 17 px high times the interface scale
MARGIN = 2  # transparent pixels around each shield: 4 between neighbours, so filtering does not bleed
MIN_ALPHA = 16
GLOW_X, GLOW_Y, GLOW_S = 144, 0, 64  # right of the 3 x 3 insignia
SPARK_X, SPARK_Y, SPARK_S = 208, 0, 24
RANK10_X, RANK10_Y = 144, 64  # under the glow
PLUS_X, PLUS_Y, PLUS_S = 200, 64, 48  # right of rank 10
PLIERS_X, PLIERS_Y, PLIERS_S = 144, 120, 48  # under rank 10


def cells(alpha):
    """The 9 shields' boxes, left to right, top to bottom."""
    labels, _ = ndimage.label(alpha > 0.5)
    boxes = [b for b in ndimage.find_objects(labels) if (b[0].stop - b[0].start) > 100]
    boxes.sort(key=lambda b: (round(b[0].start / 200), b[1].start))
    assert len(boxes) == 9, len(boxes)
    return boxes


def lum(rgb):
    return rgb @ np.array([0.299, 0.587, 0.114])


def chevron_mask(rgb, alpha):
    """Coloured metal (the chevrons), not the dark plate or the grey rim."""
    mx, mn = rgb.max(axis=2), rgb.min(axis=2)
    sat = (mx - mn) / np.maximum(mx, 1e-3)
    return (alpha > 0.5) & (sat > 0.25) & (mx > 0.25)


def silver_mask(rgb, alpha):
    """Silver chevrons: bright pixels inside the plate (the grey rim is eroded away)."""
    inner = ndimage.binary_erosion(alpha > 0.5, iterations=int(min(alpha.shape) * 0.12))
    return inner & (rgb.mean(axis=2) > 0.45)


def sorted_colours(rgb, mask):
    px = rgb[mask]
    return px[np.argsort(lum(px))]


def recolour(rgb, mask, target):
    l = lum(rgb[mask])
    q = np.argsort(np.argsort(l)) / max(len(l) - 1, 1)  # quantile of each pixel
    idx = np.clip((q * (len(target) - 1)).round().astype(int), 0, len(target) - 1)
    out = rgb.copy()
    out[mask] = target[idx]
    return out


def icons():
    """The 9 insignia as RGBA float arrays, levels 1-9."""
    sys.path.insert(0, HERE)
    from cut_sprites import key_out
    rgb = np.asarray(Image.open(SRC).convert("RGB"), dtype=np.float64) / 255.0
    color, alpha = key_out(rgb)
    crops = [(color[b], alpha[b]) for b in cells(alpha)]
    silver = np.concatenate([sorted_colours(c, silver_mask(c, a)) for c, a in crops[3:6]])
    gold = np.concatenate([sorted_colours(c, chevron_mask(c, a)) for c, a in crops[6:9]])
    targets = [None, silver[np.argsort(lum(silver))], gold[np.argsort(lum(gold))]]
    out = []
    for target in targets:
        for k in range(3):
            c, a = crops[k]  # the bronze shield with k + 1 chevrons
            if target is not None:
                c = recolour(c, chevron_mask(c, a), target)
            out.append(np.dstack([c, a]))
    out.append(rank10(out[8], targets[2]))
    return out


def rank10(rgba, gold):
    """Level 9's shield with a gold rim and a five-pointed gold star above the chevrons."""
    from PIL import ImageDraw
    c, a = rgba[..., :3].copy(), rgba[..., 3]
    h, w = a.shape
    rim = (a > 0.5) & ~ndimage.binary_erosion(a > 0.5, iterations=int(min(a.shape) * 0.09))
    chev = chevron_mask(c, a)
    c = recolour(c, rim & ~chev, gold)
    # the chevrons smaller (SHRINK) and lower, over the plate's own colour, to make room for the star
    SHRINK = 0.72
    inner = (a > 0.5) & ~rim
    plate = c.copy()
    edge = ndimage.binary_dilation(chev, iterations=int(min(a.shape) * 0.03)) & inner  # with their dark edges
    for y in range(h):
        bg = inner[y] & ~edge[y]
        if bg.any() and edge[y].any():
            plate[y, edge[y]] = np.median(c[y, bg], axis=0)
    rows, cols = np.where(chev)
    y0, y1, x0, x1 = rows.min(), rows.max() + 1, cols.min(), cols.max() + 1
    sw, sh = round((x1 - x0) * SHRINK), round((y1 - y0) * SHRINK)
    small = [np.asarray(Image.fromarray((ch * 255).astype(np.uint8)).resize((sw, sh), Image.LANCZOS),
                        dtype=np.float64) / 255.0
             for ch in (*np.moveaxis(c[y0:y1, x0:x1], 2, 0), chev[y0:y1, x0:x1].astype(np.float64))]
    nx, ny = round((w - sw) / 2), y1 - sh + round(h * 0.03)
    k = small[3][..., None] * inner[ny:ny + sh, nx:nx + sw, None]
    plate[ny:ny + sh, nx:nx + sw] = plate[ny:ny + sh, nx:nx + sw] * (1 - k) + np.dstack(small[:3]) * k
    c = plate
    top = ny  # the highest chevron now
    cx, cy = w / 2, (top + h * 0.1) / 2
    r_out = min(w * 0.2, (top - h * 0.1) * 0.46)
    pts = [(cx + (r_out if k % 2 == 0 else r_out * 0.42) * np.sin(k * np.pi / 5),
            cy - (r_out if k % 2 == 0 else r_out * 0.42) * np.cos(k * np.pi / 5)) for k in range(10)]
    big = Image.new("L", (w * 4, h * 4))
    ImageDraw.Draw(big).polygon([(x * 4, y * 4) for x, y in pts], fill=255)
    star = np.asarray(big.resize((w, h), Image.LANCZOS), dtype=np.float64) / 255.0
    ys = np.clip((np.arange(h) - (cy - r_out)) / max(2 * r_out, 1), 0, 1)  # light at the top
    shade = gold[np.clip(((1 - ys) * 0.8 + 0.2) * (len(gold) - 1), 0, len(gold) - 1).astype(int)]
    c = c * (1 - star[..., None]) + shade[:, None, :] * star[..., None]
    return np.dstack([c, a])


def shrink(rgba):
    """To fit CELL inside MARGIN with premultiplied alpha, centred; faint edge pixels dropped."""
    h, w = rgba.shape[:2]
    k = min((CELL_W - 2 * MARGIN) / w, (CELL_H - 2 * MARGIN) / h)
    sw, sh = max(1, round(w * k)), max(1, round(h * k))
    alpha = rgba[..., 3]
    pre = np.dstack([rgba[..., :3] * alpha[..., None], alpha])
    small = np.stack([np.asarray(Image.fromarray((pre[..., c] * 255).astype(np.uint8)).resize(
        (sw, sh), Image.LANCZOS)).astype(float) / 255 for c in range(4)], axis=2)
    a = np.clip(small[..., 3:], 1e-6, 1)
    out = np.dstack([np.clip(small[..., :3] / a, 0, 1), small[..., 3:]])
    out[out[..., 3] < MIN_ALPHA / 255] = 0
    cell = Image.new("RGBA", (CELL_W, CELL_H))
    cell.paste(Image.fromarray((out * 255 + 0.5).astype(np.uint8), "RGBA"), ((CELL_W - sw) // 2, (CELL_H - sh) // 2))
    return cell


def white(alpha):
    """A white image with this alpha (0..1); the game tints it."""
    a = (np.clip(alpha, 0, 1) * 255 + 0.5).astype(np.uint8)
    rgba = np.dstack([np.full_like(a, 255)] * 3 + [a])
    rgba[a == 0] = 0
    return Image.fromarray(rgba, "RGBA")


def grid(size):
    """Pixel centres in -1..1, and their radius and angle."""
    c = (np.arange(size) + 0.5) / size * 2 - 1
    x, y = np.meshgrid(c, c)
    return x, y, np.hypot(x, y), np.arctan2(y, x)


def glow():
    """Soft round glow, brightest in the middle, with 8 faint rays; 0 at the edge."""
    _, _, r, t = grid(GLOW_S - 2 * MARGIN)
    edge = np.clip(1 - r, 0, 1)
    core = np.exp(-(r / 0.42) ** 2)
    rays = np.abs(np.cos(4 * t)) ** 6 * np.exp(-(r / 0.75) ** 2) * 0.35
    cell = Image.new("RGBA", (GLOW_S, GLOW_S))
    cell.paste(white((core + rays) * edge ** 0.5), (MARGIN, MARGIN))
    return cell


def spark():
    """Four-pointed star: two thin crossed streaks and a bright centre."""
    x, y, r, _ = grid(SPARK_S - 2 * MARGIN)
    streaks = np.exp(-(x / 0.09) ** 2 - (y / 0.75) ** 2) + np.exp(-(y / 0.09) ** 2 - (x / 0.75) ** 2)
    centre = np.exp(-(r / 0.28) ** 2)
    cell = Image.new("RGBA", (SPARK_S, SPARK_S))
    cell.paste(white(np.maximum(streaks, centre) * np.clip(1 - r, 0, 1)), (MARGIN, MARGIN))
    return cell


def plus():
    """Rounded cross, white inside a dark rim (the rim stays dark when the game tints it)."""
    x, y, r, _ = grid(PLUS_S - 2 * MARGIN)
    arm, length = 0.24, 0.80
    # distance outside the cross (0 inside), with rounded ends
    dx = np.maximum(np.abs(x) - arm, 0), np.maximum(np.abs(y) - length, 0)
    dy = np.maximum(np.abs(y) - arm, 0), np.maximum(np.abs(x) - length, 0)
    d = np.minimum(np.hypot(*dx), np.hypot(*dy))
    soft = 2.0 / (PLUS_S - 2 * MARGIN)  # a pixel's width in -1..1, for the anti-aliased edges
    inner = np.clip(1 - d / soft, 0, 1)
    rim = np.clip((0.16 - d) / soft, 0, 1)
    shade = 0.15 + 0.85 * inner
    a = (np.clip(np.maximum(rim, inner), 0, 1) * 255 + 0.5).astype(np.uint8)
    v = (np.clip(shade, 0, 1) * 255 + 0.5).astype(np.uint8)
    rgba = np.dstack([v, v, v, a])
    rgba[a == 0] = 0
    cell = Image.new("RGBA", (PLUS_S, PLUS_S))
    cell.paste(Image.fromarray(rgba, "RGBA"), (MARGIN, MARGIN))
    return cell


def pliers():
    """Steel pliers with red grips, jaws up and to the right, in a dark rim (not tinted)."""
    from PIL import ImageDraw
    big = 384
    def shape(points):
        m = Image.new("L", (big, big))
        ImageDraw.Draw(m).polygon([(x * big, y * big) for x, y in points], fill=255)
        return np.asarray(m, np.float32) / 255
    def disc(cx, cy, r):
        m = Image.new("L", (big, big))
        ImageDraw.Draw(m).ellipse([(cx - r) * big, (cy - r) * big, (cx + r) * big, (cy + r) * big], fill=255)
        return np.asarray(m, np.float32) / 255
    # upright: pivot at (0.5, 0.42), jaws closed above it, handles spread below
    jaw_l = shape([(0.50, 0.47), (0.40, 0.40), (0.43, 0.24), (0.47, 0.10), (0.50, 0.06)])
    jaw_r = shape([(0.50, 0.47), (0.60, 0.40), (0.57, 0.24), (0.53, 0.10), (0.50, 0.06)])
    arm_l = shape([(0.43, 0.44), (0.52, 0.48), (0.29, 0.94), (0.19, 0.91)])
    arm_r = shape([(0.57, 0.44), (0.48, 0.48), (0.71, 0.94), (0.81, 0.91)])
    grip_l = shape([(0.375, 0.64), (0.445, 0.665), (0.29, 0.955), (0.17, 0.92)])
    grip_r = shape([(0.625, 0.64), (0.555, 0.665), (0.71, 0.955), (0.83, 0.92)])
    bolt = disc(0.5, 0.44, 0.055)
    body = np.clip(jaw_l + jaw_r + arm_l + arm_r + grip_l + grip_r + bolt, 0, 1)
    rim = ndimage.maximum_filter(body, size=int(big * 0.07))
    rgb = np.zeros((big, big, 3), np.float32)
    rgb[:] = (24, 22, 20)  # the rim
    yy = np.linspace(0, 1, big)[:, None]
    steel = np.clip(150 + 90 * (1 - yy), 0, 255)  # brighter at the top
    for m, k in ((jaw_l, 0.80), (jaw_r, 1.0), (arm_l, 0.72), (arm_r, 0.92)):
        for c in range(3):
            rgb[..., c] = rgb[..., c] * (1 - m) + steel[:, 0][:, None] * k * m
    for m, k in ((grip_l, 0.78), (grip_r, 1.0)):
        for c, v in enumerate((196, 46, 34)):
            rgb[..., c] = rgb[..., c] * (1 - m) + v * k * m
    for c, v in enumerate((70, 70, 74)):
        rgb[..., c] = rgb[..., c] * (1 - bolt) + v * bolt
    seam = shape([(0.494, 0.40), (0.506, 0.40), (0.503, 0.08), (0.497, 0.08)])  # between the closed jaws
    for c in range(3):
        rgb[..., c] = rgb[..., c] * (1 - seam) + 30 * seam
    shine = disc(0.515, 0.425, 0.018)
    for c in range(3):
        rgb[..., c] = rgb[..., c] * (1 - shine) + 230 * shine
    rgba = np.dstack([rgb, rim * 255]).astype(np.uint8)
    img = Image.fromarray(rgba, "RGBA").rotate(-25, resample=Image.BICUBIC)
    img = img.crop(img.getbbox())
    inner = PLIERS_S - 2 * MARGIN
    k = inner / max(img.width, img.height)
    img = img.resize((max(1, round(img.width * k)), max(1, round(img.height * k))), Image.LANCZOS)
    cell = Image.new("RGBA", (PLIERS_S, PLIERS_S))
    cell.paste(img, ((PLIERS_S - img.width) // 2, (PLIERS_S - img.height) // 2))
    a = np.asarray(cell).copy()
    a[a[..., 3] < MIN_ALPHA] = 0
    return Image.fromarray(a, "RGBA")


def build():
    atlas = Image.new("RGBA", (256, 256))
    for n, rgba in enumerate(icons()):
        atlas.paste(shrink(rgba), ((n % 3) * CELL_W, (n // 3) * CELL_H) if n < 9 else (RANK10_X, RANK10_Y))
    atlas.paste(glow(), (GLOW_X, GLOW_Y))
    atlas.paste(spark(), (SPARK_X, SPARK_Y))
    atlas.paste(plus(), (PLUS_X, PLUS_Y))
    atlas.paste(pliers(), (PLIERS_X, PLIERS_Y))
    return atlas


def tex_lines():
    return ['<IFACETEXLOAD id="TEX_REVIVED_RANKS" file=".\\%s" pixformat="PF_32"/>' % ATLAS] + [
        f'<IFACETEX name="rank_{n + 1}" texture="TEX_REVIVED_RANKS" '
        f'left="{(n % 3) * CELL_W}" top="{(n // 3) * CELL_H}" width="{CELL_W}" height="{CELL_H}"/>'
        for n in range(9)] + [
        f'<IFACETEX name="rank_10" texture="TEX_REVIVED_RANKS" '
        f'left="{RANK10_X}" top="{RANK10_Y}" width="{CELL_W}" height="{CELL_H}"/>',
        f'<IFACETEX name="rank_glow" texture="TEX_REVIVED_RANKS" '
        f'left="{GLOW_X}" top="{GLOW_Y}" width="{GLOW_S}" height="{GLOW_S}"/>',
        f'<IFACETEX name="rank_spark" texture="TEX_REVIVED_RANKS" '
        f'left="{SPARK_X}" top="{SPARK_Y}" width="{SPARK_S}" height="{SPARK_S}"/>',
        f'<IFACETEX name="heal_plus" texture="TEX_REVIVED_RANKS" '
        f'left="{PLUS_X}" top="{PLUS_Y}" width="{PLUS_S}" height="{PLUS_S}"/>',
        f'<IFACETEX name="repair_pliers" texture="TEX_REVIVED_RANKS" '
        f'left="{PLIERS_X}" top="{PLIERS_Y}" width="{PLIERS_S}" height="{PLIERS_S}"/>']


def main(out):
    sys.path.insert(0, HERE)
    from build_menu_theme import write_tga
    atlas = build()
    write_tga(atlas, os.path.join(out, ATLAS))
    prev = Image.new("RGBA", (PLUS_X + PLUS_S, 3 * CELL_H), (40, 44, 50, 255))
    prev.alpha_composite(atlas.crop((0, 0, PLUS_X + PLUS_S, 3 * CELL_H)))
    prev.resize((prev.width * 2, prev.height * 2), Image.NEAREST).save(
        os.path.join(HERE, "..", "preview_rank_icons.png"))


if __name__ == "__main__":
    main(sys.argv[1])
