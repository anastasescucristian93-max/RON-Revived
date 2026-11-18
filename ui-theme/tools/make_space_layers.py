r"""Splits the space scene into the menu's five parallax layers.

    python make_space_layers.py

Inputs (ui-theme\source\space_scene), Gemini edits of one picture, each removing one more thing,
so they line up:
  gemini_0_scene.webp          the whole scene: space station, astronaut, Earth, missile, sun
  gemini_1_no_astronaut.webp   the astronaut and his tether removed
  gemini_2_no_station.webp     the station removed too
  gemini_3_no_missile.webp     the missile removed too (its launch glow stays on the ground)
  gemini_4_background.webp     the Earth removed too: stars and the sun
  station_matte.png            rmbg_matte.py of gemini_1 (the station)
  astronaut_matte.png          rmbg_matte.py of gemini_0 (astronaut and station)

Outputs (source\space_scene\hd, read by build_menu_theme.py), back to front:
  layer_0_background.png  stars and sun, opaque; under the Earth (where Gemini left traces of
                          it) the stars of the upper sky, so a moving Earth uncovers only sky
  layer_1_earth.png       the Earth, opaque inside its limb (a circle fitted to it), the
                          atmosphere's glow above the limb from the difference to the background
  layer_2_missile.png     the missile trail, from the difference to the no-missile picture
  layer_3_station.png     the station, cut with its matte (dark solar panels on dark space give
                          no difference, the matte holds them)
  layer_4_astronaut.png   the astronaut (matte, kept only where the astronaut was removed) and
                          his tether (difference)
"""
import os

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "source", "space_scene")
OUT = os.path.join(SRC, "hd")
# Compression noise between the edits: below NOISE a difference layer is transparent, it is
# opaque from NOISE + RAMP up.
NOISE, RAMP = 14.0, 40.0
# The Earth's limb: where a column first rises to this share of its brightest step over the
# background (the glow above the limb is less).
LIMB = 0.55


def load(name):
    return np.asarray(Image.open(os.path.join(SRC, name)).convert("RGB"), np.float64)


def matte(name):
    return np.asarray(Image.open(os.path.join(SRC, name)).convert("L"), np.float64) / 255


def save(rgb, alpha, name):
    os.makedirs(OUT, exist_ok=True)
    a = np.clip(alpha, 0, 1)[..., None] * 255
    img = Image.fromarray(np.dstack([np.clip(rgb, 0, 255), a]).round().astype(np.uint8), "RGBA")
    img.save(os.path.join(OUT, name))
    print(os.path.join(OUT, name), img.size)


def over(top, under, noise=NOISE, ramp=RAMP):
    """The layer of what `top` adds over `under`: alpha from the difference, the colour solved so
    that alpha * colour + (1 - alpha) * under gives `top` again."""
    d = np.abs(top - under).max(axis=2)
    alpha = np.clip((d - noise) / ramp, 0, 1)
    alpha = ndimage.gaussian_filter(alpha, 0.8)
    safe = np.maximum(alpha, 1e-3)[..., None]
    color = np.where(alpha[..., None] > 0.02, under + (top - under) / safe, top)
    return color, alpha


def largest(mask):
    labels, n = ndimage.label(mask)
    if n == 0:
        return mask
    sizes = ndimage.sum(np.ones(mask.shape), labels, range(1, n + 1))
    return labels == 1 + int(np.argmax(sizes))


def earth_limb(earth, bg):
    """The row of the Earth's limb in every column: per column the first row from the top where
    the Earth picture rises to LIMB of its brightest step over the background (the bright rim,
    not the faint glow above it; the left side is darker, so a share, not a level), then a
    smooth curve through those rows (the limb is not a circle in the picture's perspective)."""
    h, w = bg.shape[:2]
    d = ndimage.gaussian_filter((earth - bg).max(axis=2), 1.5)
    top, bottom = 100, int(h * 0.7)
    xs, ys = [], []
    for x in range(8, w - 8, 4):
        col = d[top:bottom, x]
        rows = np.nonzero(col >= LIMB * col.max())[0]
        if col.max() > 30 and len(rows):
            xs.append(x)
            ys.append(top + rows[0])
    xs, ys = np.array(xs, float), np.array(ys, float)
    keep = np.ones(len(xs), bool)
    for _ in range(4):  # fit, drop the points far off the curve, fit again
        p = np.polyfit(xs[keep], ys[keep], 4)
        err = np.abs(np.polyval(p, xs) - ys)
        keep = err < max(2.0, np.percentile(err[keep], 85))
    return np.polyval(p, np.arange(w))


def main():
    scene = load("gemini_0_scene.webp")
    no_astro = load("gemini_1_no_astronaut.webp")
    no_station = load("gemini_2_no_station.webp")
    no_missile = load("gemini_3_no_missile.webp")
    bg = load("gemini_4_background.webp")
    shapes = {p.shape for p in (scene, no_astro, no_station, no_missile, bg)}
    assert len(shapes) == 1, shapes
    h, w = bg.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]

    # Earth: opaque inside the limb (a little inside, the limb's own edge comes from the
    # difference), the glow above it from the difference to the background.
    limb = earth_limb(no_missile, bg)
    print(f"Earth limb: row {limb[0]:.0f} at the left, {limb.min():.0f} at the top, {limb[-1]:.0f} at the right")
    below = yy - limb[None, :]  # rows below the limb
    inside = np.clip((below - 2) / 4, 0, 1)
    # The haze above the limb is faint (on the left only some 20 levels over the sky), so a low
    # noise level here, or it ends in a hard edge.
    glow_color, glow_alpha = over(no_missile, bg, noise=4, ramp=30)
    glow_alpha = glow_alpha * (below > -40)
    alpha = np.maximum(inside, glow_alpha)
    color = np.where(inside[..., None] >= 1, no_missile, glow_color)
    save(color, alpha, "layer_1_earth.png")

    # Background: under the Earth, the dark sky colour with the upper sky's stars (only their
    # fine detail, mirrored), so whatever a moving Earth uncovers is sky, not Gemini's leftovers.
    sky_rows = max(40, int(limb.min()) - 20)
    sky = bg[:sky_rows]
    base = np.median(sky[:, : w // 2].reshape(-1, 3), axis=0)
    stars = np.clip(sky - ndimage.median_filter(sky, size=(7, 7, 1)), 0, None)
    stars = np.concatenate([stars, stars[::-1]] * (h // (2 * sky_rows) + 1))[:h]
    filled = base + stars
    under = np.clip((below - 10) / 20, 0, 1)[..., None]
    save(bg * (1 - under) + filled * under, np.ones((h, w)), "layer_0_background.png")

    # Missile: the trail over the Earth.
    color, alpha = over(no_station, no_missile)
    alpha = alpha * ndimage.binary_dilation(largest(alpha > 0.5), iterations=8)
    save(color, alpha, "layer_2_missile.png")

    # Station: the matte, its largest part.
    m = matte("station_matte.png")
    keep = ndimage.binary_dilation(largest(m > 0.5), iterations=6)
    save(no_astro, m * keep, "layer_3_station.png")

    # Astronaut: the matte of the whole scene only where the edit changed a lot (the white suit,
    # not the station's panel behind his arm), and the thin tether from the difference: only its
    # fine detail (the soft glow Gemini took away with him would move with him as a smudge), and
    # only left of and beside him (elsewhere the difference is compression noise in the lights).
    d_color, d_alpha = over(scene, no_astro)
    d = np.abs(scene - no_astro).max(axis=2)
    core = largest(d > 60)
    ys, xs = np.nonzero(core)
    box = ((xx >= xs.min() - 60) & (xx <= xs.max() + 60) & (yy >= ys.min() - 60) & (yy <= ys.max() + 60))
    labels, n = ndimage.label((d > 35) & box)
    sizes = ndimage.sum(np.ones(d.shape), labels, range(1, n + 1))
    suit = np.isin(labels, 1 + np.nonzero(sizes >= 20)[0])
    suit = ndimage.binary_fill_holes(ndimage.binary_dilation(suit, iterations=6))
    body = matte("astronaut_matte.png") * suit
    ys, xs = np.nonzero(core)
    # The tether hangs to his left; city lights there differ too, but in small round specks, the
    # tether in long pieces.
    near = (xx <= xs.min() + 20) & (yy >= ys.min() - 30) & (yy <= ys.max() + 30)
    fine = (d - ndimage.gaussian_filter(d, 3)) > 12
    labels, n = ndimage.label(ndimage.binary_dilation(fine & near, iterations=1))
    thread = np.zeros(d.shape, bool)
    for k, sl in enumerate(ndimage.find_objects(labels), 1):
        if max(sl[0].stop - sl[0].start, sl[1].stop - sl[1].start) >= 14:
            thread |= labels == k
    tether = d_alpha * thread
    alpha = np.maximum(body, tether)
    # The picture's own colours: the solved ones run wild where the alpha is small.
    save(scene, alpha, "layer_4_astronaut.png")


if __name__ == "__main__":
    main()
