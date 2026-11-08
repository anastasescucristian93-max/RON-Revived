r"""The nuke target marker, drawn in code (no picture): the radiation sign in parts, a disc in the
middle and three blades, which nukefx.c draws apart and pulls in towards the disc while the nuke
flies, so the sign closes the moment it lands (the user, 2026-11-08: "un cerc in mijloc si cele 3
parti exterioare ... sa se stranga catre centru pana cand cade nuke", "fara cercuri"). No rings and
no dark disc under it.

    python make_target_decal.py   ->  art\nuke_target.tga, art\nuke_target_glow.tga   (one blade)
                                      art\nuke_core.tga, art\nuke_core_glow.tga       (the disc)
                                      + work\nuke_target_preview.png (the closing, on sand and grass)

All four are 512 x 512 with the sign's centre in the middle of the canvas, so the DLL draws every
part with the same width; the blade lies on the canvas's right (+u), its axis on the middle row,
so it is the same picture upside down (the game's row order does not matter). Proportions of the
standard trefoil: disc radius R, blades from 1.5 R to 5 R, 60 degrees wide.

Two layers per part:
- the shape (NukeTarget / NukeCore, normal blending): yellow, a dark rim and a soft dark shadow
  that follows the shape, so it reads on bright sand;
- the glow (NukeTargetGlow / NukeCoreGlow, Shockwave Blend, adds light): the shape blurred,
  yellow-green, pulsing.
"""
import os

import numpy as np
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.dirname(HERE)
S = 512
SS = 4                  # supersampling for smooth edges
R = 46.0                # disc radius in texture pixels; blades reach 5 R = 230
BLADE_IN, BLADE_OUT, HALF_ANGLE = 1.5 * R, 5.0 * R, 30.0
YELLOW = np.array([255.0, 212.0, 28.0])
RIM = np.array([28.0, 22.0, 8.0])
GLOW = np.array([215.0, 255.0, 70.0])


def mask(kind):
    n = S * SS
    yy, xx = np.mgrid[0:n, 0:n]
    c = (n - 1) / 2
    x, y = (xx - c) / SS, (yy - c) / SS
    d = np.hypot(x, y)
    if kind == "core":
        m = d <= R
    else:
        ang = np.degrees(np.abs(np.arctan2(y, x)))
        m = (d >= BLADE_IN) & (d <= BLADE_OUT) & (ang <= HALF_ANGLE)
    img = Image.fromarray((m * 255).astype(np.uint8)).resize((S, S), Image.LANCZOS)
    return np.asarray(img, dtype=np.float64) / 255


def blur(a, r):
    return np.asarray(Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(r)),
                      dtype=np.float64) / 255


def erode(a, px):
    return np.asarray(Image.fromarray((a * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(2 * px + 1)),
                      dtype=np.float64) / 255


def layers(kind):
    m = mask(kind)
    inner = erode(m, 4)                                  # the yellow; the 4 px around it is the rim
    shadow = np.clip(blur(m, 7) * 1.4, 0, 1) * 0.45      # follows the shape, no disc
    # the yellow a touch lighter towards its middle
    shade = 0.88 + 0.12 * blur(erode(m, 10), 8)
    a = np.maximum(m * 0.92, shadow)
    a = np.maximum(a, inner)
    rgb = YELLOW * shade[..., None] * inner[..., None] + RIM * (1 - inner[..., None])
    shape = np.dstack([np.clip(rgb, 0, 255), a * 255]).astype(np.uint8)

    gi = np.clip(0.45 * inner + 1.1 * blur(m, 10), 0, 1)
    glow = np.dstack([GLOW * gi[..., None], gi * 255]).astype(np.uint8)
    return shape, glow


P = 2 * S  # preview canvas


def place(rgba, dx, dy):
    """rgba (S x S) on a transparent P x P canvas, its centre moved by dx, dy from the middle."""
    a = np.zeros((P, P, 4))
    x0, y0 = (P - S) // 2 + dx, (P - S) // 2 + dy
    a[y0:y0 + S, x0:x0 + S] = rgba
    return a


def over(ground, rgba, dx=0, dy=0):
    a = place(rgba.astype(np.float64), dx, dy)
    al = a[..., 3:4] / 255
    return ground * (1 - al) + a[..., :3] * al


def add(ground, rgba, k, dx=0, dy=0):
    a = place(rgba.astype(np.float64), dx, dy)
    return np.clip(ground + a[..., :3] * (a[..., 3:4] / 255) * k, 0, 255)


def main():
    art = os.path.join(MOD, "art")
    os.makedirs(art, exist_ok=True)
    blade, blade_glow = layers("blade")
    core, core_glow = layers("core")
    for name, im in (("nuke_target", blade), ("nuke_target_glow", blade_glow),
                     ("nuke_core", core), ("nuke_core_glow", core_glow)):
        Image.fromarray(im, "RGBA").save(os.path.join(art, name + ".tga"))

    # preview: the closing at 0 %, 50 %, 85 % and 100 % of the flight, on sand and on grass, top
    # down (the DLL: blades pushed out by 0.8 of the sign's half width at the start, to 0 at the end)
    rows = []
    for ground in ((214, 188, 140), (86, 112, 58)):
        tiles = []
        for f in (0.0, 0.5, 0.85, 1.0):
            g = np.ones((P, P, 3)) * np.array(ground, dtype=np.float64)
            off = 0.8 * (S / 2) * (1 - f)
            parts = []
            for k in range(3):
                ang = np.radians(90 + 120 * k)
                rot = Image.fromarray(blade).rotate(np.degrees(ang), resample=Image.BICUBIC)
                rotg = Image.fromarray(blade_glow).rotate(np.degrees(ang), resample=Image.BICUBIC)
                dx, dy = int(round(off * np.cos(ang))), int(round(-off * np.sin(ang)))
                parts.append((np.asarray(rot), np.asarray(rotg), dx, dy))
            for p, _, dx, dy in parts:
                g = over(g, p, dx, dy)
            g = over(g, core)
            for _, pg, dx, dy in parts:
                g = add(g, pg, 0.8, dx, dy)
            g = add(g, core_glow, 0.8)
            tiles.append(g)
        rows.append(np.hstack(tiles))
    os.makedirs(os.path.join(MOD, "work"), exist_ok=True)
    prev = Image.fromarray(np.vstack(rows).astype(np.uint8))
    prev = prev.resize((prev.width // 4, prev.height // 4), Image.LANCZOS)
    prev.save(os.path.join(MOD, "work", "nuke_target_preview.png"))
    print("art\\nuke_target(_glow).tga, art\\nuke_core(_glow).tga, work\\nuke_target_preview.png")


if __name__ == "__main__":
    main()
