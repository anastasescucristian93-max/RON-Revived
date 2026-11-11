r"""The pieces of the Survival wave bar (camera-smooth\src\survival.c): the row of circles at the
top of the screen, one per wave, like the wave bars of survival and tower defence games (the
user, 2026-11-10).

    python build_survival_bar.py <out mod_override folder>

Every piece is white or grey, so the DLL tints it with the colour it draws it in (the
interface multiplies the texture by the colour): a past wave grey, the next one amber, a boss
wave red.

  surv_disc  a filled disc, a little darker towards its edge
  surv_ring  a steel rim, lit from the upper right like the rest of the interface
  surv_glow  a soft round glow, drawn behind the next wave
  surv_pip   a small triangle pointing down: where the game is now on a horizontal bar
  surv_pip_r the same pointing right, for the vertical bar at the right edge of the screen
  hero_infantry, hero_cavalry  the two heroes' medallions (Gemini, 2026-11-10: a gold infantryman /
             rider on a black disc with a gold rim, source\hero_icons\, cut and cleaned there),
             HERO_CELL square, in full colour (drawn white): the Survival setup's hero buttons and
             the mark over the hero's head

Output: art\revived\survival_bar.tga, declared in interface.xml by build_menu_theme.py
(tex_lines below; IFACEMAINBASE, where the DLL looks its textures up).
"""
import os
import struct
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
HEROES = ("hero_infantry", "hero_cavalry")
HERO_CELL = 128
HERO_DIR = os.path.join(HERE, "..", "source", "hero_icons")
ATLAS = "art\\revived\\survival_bar.tga"
CELL = 64
SS = 4  # supersampling
NAMES = ("surv_disc", "surv_ring", "surv_glow", "surv_pip", "surv_pip_r")


def grid():
    n = CELL * SS
    yy, xx = np.mgrid[0:n, 0:n].astype(float)
    c = (n - 1) / 2
    return (xx - c) / SS, (yy - c) / SS  # in cell pixels, from the centre


def down(rgba):
    """Supersampled float RGBA to a CELL x CELL image, premultiplied while it shrinks."""
    pre = np.dstack([rgba[..., :3] * rgba[..., 3:], rgba[..., 3:]])
    small = pre.reshape(CELL, SS, CELL, SS, 4).mean(axis=(1, 3))
    a = np.clip(small[..., 3:], 1e-6, 1)
    out = np.dstack([np.clip(small[..., :3] / a, 0, 1), small[..., 3:]])
    return Image.fromarray((out * 255 + 0.5).astype(np.uint8), "RGBA")


def disc():
    x, y = grid()
    r = np.hypot(x, y)
    a = np.clip(29.5 - r, 0, 1)
    shade = 1.0 - 0.22 * np.clip(r / 29.5, 0, 1) ** 2
    return down(np.dstack([shade] * 3 + [a]))


def ring():
    x, y = grid()
    r = np.hypot(x, y)
    a = np.clip(31.5 - r, 0, 1) * np.clip(r - 22.5, 0, 1)
    # light from the upper right: brighter where the rim faces it, a thin bright edge outside
    face = (x - y) / np.maximum(r, 1e-6) / np.sqrt(2)
    tone = 0.62 + 0.30 * face
    tone = np.where(np.abs(r - 30.0) < 1.2, tone + 0.18, tone)
    tone = np.where(np.abs(r - 23.8) < 1.0, tone - 0.20, tone)
    tone = np.clip(tone, 0, 1)
    return down(np.dstack([tone] * 3 + [a]))


def glow():
    x, y = grid()
    r = np.hypot(x, y) / 31.5
    a = np.clip(1 - r, 0, 1) ** 2
    return down(np.dstack([np.ones_like(a)] * 3 + [a]))


def pip():
    x, y = grid()
    # a triangle pointing down: top edge at y -20, tip at y +18, half width 18 at the top
    half = 18 * (18 - y) / 38
    a = np.clip(half - np.abs(x), 0, 1) * np.clip(y + 20, 0, 1) * np.clip(18 - y, 0, 1)
    tone = 1.0 - 0.25 * np.clip((y + 20) / 38, 0, 1)
    return down(np.dstack([tone] * 3 + [a]))


def pip_right():
    return pip().transpose(Image.Transpose.ROTATE_90)


def build():
    atlas = Image.new("RGBA", (max(CELL * len(NAMES), HERO_CELL * len(HEROES)), CELL + HERO_CELL))
    for i, name in enumerate(HEROES):
        path = os.path.join(HERO_DIR, name + ".png")
        if os.path.exists(path):
            icon = Image.open(path).convert("RGBA").resize((HERO_CELL, HERO_CELL), Image.LANCZOS)
            atlas.paste(icon, (i * HERO_CELL, CELL))
    for i, piece in enumerate((disc(), ring(), glow(), pip(), pip_right())):
        atlas.paste(piece, (i * CELL, 0))
    return atlas


def tex_lines():
    return ['<IFACETEXLOAD id="TEX_REVIVED_SURVIVAL" file=".\\%s" pixformat="PF_32"/>' % ATLAS] + [
        f'<IFACETEX name="{name}" texture="TEX_REVIVED_SURVIVAL" left="{i * CELL}" top="0" '
        f'width="{CELL}" height="{CELL}"/>' for i, name in enumerate(NAMES)] + [
        f'<IFACETEX name="{name}" texture="TEX_REVIVED_SURVIVAL" left="{i * HERO_CELL}" top="{CELL}" '
        f'width="{HERO_CELL}" height="{HERO_CELL}"/>' for i, name in enumerate(HEROES)]


def preview(atlas, path):
    """The bar roughly as the DLL draws it: grey past waves, the amber next one, red bosses."""
    w, h = 760, 70
    img = Image.new("RGBA", (w, h), (52, 60, 46, 255))
    pieces = [atlas.crop((i * CELL, 0, (i + 1) * CELL, CELL)) for i in range(len(NAMES))]

    def put(i, cx, cy, size, rgb, alpha=255):
        p = pieces[i].resize((size, size), Image.LANCZOS)
        a = np.asarray(p).astype(float) / 255
        a[..., :3] *= np.array(rgb) / 255
        a[..., 3] *= alpha / 255
        img.alpha_composite(Image.fromarray((a * 255).astype(np.uint8), "RGBA"), (cx - size // 2, cy - size // 2))

    track = Image.new("RGBA", (w - 60, 6), (10, 10, 10, 200))
    img.alpha_composite(track, (30, 32))
    img.alpha_composite(Image.new("RGBA", (220, 4), (232, 170, 60, 255)), (30, 33))
    xs = [30 + int(i * (w - 60) / 26) for i in range(27)]
    for i, x in enumerate(xs):
        boss = i % 6 == 5
        past, nxt = x < 250, xs[i - 1] < 250 <= x if i else False
        size = 22 if boss else 14
        if nxt:
            put(2, x, 35, 40, (255, 190, 80), 170)
        col = (120, 120, 120) if past else (205, 40, 30) if boss else (232, 170, 60) if nxt else (60, 52, 40)
        put(0, x, 35, size, col)
        put(1, x, 35, size, (200, 200, 200) if not past else (130, 130, 130))
    put(3, 250, 22, 14, (255, 255, 255))
    img.resize((w * 2, h * 2), Image.LANCZOS).save(path)


def write_tga(img, path):
    """Uncompressed 32-bit, bottom-up, as build_menu_theme.write_tga (kept here so this file runs
    on its own: importing build_menu_theme from here is circular)."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    w, h = img.size
    a = np.asarray(img.convert("RGBA"))[::-1]
    with open(path, "wb") as f:
        f.write(bytes([0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0]) + struct.pack("<HHBB", w, h, 32, 8)
                + a[..., [2, 1, 0, 3]].tobytes())
    print(f"{path}: {w}x{h}")


def main(out):
    atlas = build()
    write_tga(atlas, os.path.join(out, ATLAS))
    preview(atlas, os.path.join(HERE, "..", "preview_survival_bar.png"))


if __name__ == "__main__":
    sys.path.insert(0, HERE)
    main(sys.argv[1])
