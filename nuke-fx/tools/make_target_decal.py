r"""The nuke target marker, the ground decal over a nuke's target, from the Gemini picture
(source\target_sign_gemini.jpg: the sign on pure magenta, with its glow mixed into the magenta).

    python make_target_decal.py   ->  art\nuke_target.tga, art\nuke_target_glow.tga (512 x 512)
                                      + work\nuke_target_preview.png (over sand and grass)

The magenta is taken out by un-mixing every pixel from it: a pixel C = a F + (1 - a) M with
M = (255, 0, 255); a is the smallest alpha that keeps F within 0..255 (the largest of
(255 - R) / 255, G / 255, (255 - B) / 255), then F = (C - (1 - a) M) / a. So the pink glow lines
become white, see-through lines, and the dark ring stays dark.

Two textures, drawn together by nukefx.c:
- nuke_target.tga (NukeTarget, normal blending): the sign over a dark, see-through disc, so it
  stands out on bright sand;
- nuke_target_glow.tga (NukeTargetGlow, the game's Shockwave Blend, which adds light): the bright
  parts of the sign, blurred, so they glow.
"""
import os

import numpy as np
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.dirname(HERE)
SRC = os.path.join(MOD, "source", "target_sign_gemini.jpg")
S = 512
BACKDROP = 0.38   # alpha of the dark disc under the sign
NOISE = 0.10      # un-mixed alpha below this is jpeg noise in the magenta


def unmix(rgb):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    a = np.maximum.reduce([(255 - r) / 255, g / 255, (255 - b) / 255])
    a = np.clip((a - NOISE) / (1 - NOISE), 0, 1)
    m = np.array([255.0, 0.0, 255.0])
    safe = np.maximum(a, 1e-3)[..., None]
    f = np.clip((rgb - (1 - a[..., None]) * m) / safe, 0, 255)
    return f, a


def main():
    img = Image.open(SRC).convert("RGB").resize((S, S), Image.LANCZOS)
    rgb = np.asarray(img, dtype=np.float64)
    f, a = unmix(rgb)

    # the sign's outer edge (the dark ring): a disc mask with a soft rim, nothing outside it
    yy, xx = np.mgrid[0:S, 0:S]
    c = (S - 1) / 2
    d = np.hypot(xx - c, yy - c) / (S / 2)
    rim = np.clip((0.955 - d) / 0.02, 0, 1)
    a *= rim

    # the decal: the sign over a dark disc
    back = BACKDROP * np.clip((0.95 - d) / 0.03, 0, 1)
    out_a = a + back * (1 - a)
    out_rgb = (f * a[..., None] + np.array([8.0, 6.0, 4.0]) * (back * (1 - a))[..., None]) / np.maximum(out_a, 1e-3)[..., None]
    decal = np.dstack([np.clip(out_rgb, 0, 255), out_a * 255]).astype(np.uint8)

    # the glow: the bright parts (the yellow blades, the light lines), blurred and added on top
    lum = (0.3 * f[..., 0] + 0.59 * f[..., 1] + 0.11 * f[..., 2]) / 255
    bright = np.clip((lum - 0.45) / 0.4, 0, 1) * a
    core = Image.fromarray((bright * 255).astype(np.uint8))
    halo = np.asarray(core.filter(ImageFilter.GaussianBlur(9)), dtype=np.float64) / 255
    glow_i = np.clip(0.55 * bright + 1.3 * halo, 0, 1) * rim
    tint = np.where(bright[..., None] > 0.05, f, np.array([220.0, 255.0, 90.0]))
    tint = np.asarray(Image.fromarray(tint.astype(np.uint8)).filter(ImageFilter.GaussianBlur(4)), dtype=np.float64)
    glow = np.dstack([tint * glow_i[..., None], glow_i * 255]).astype(np.uint8)

    art = os.path.join(MOD, "art")
    os.makedirs(art, exist_ok=True)
    Image.fromarray(decal, "RGBA").save(os.path.join(art, "nuke_target.tga"))
    Image.fromarray(glow, "RGBA").save(os.path.join(art, "nuke_target_glow.tga"))

    # preview: over sand and over grass, the glow added as the game would
    os.makedirs(os.path.join(MOD, "work"), exist_ok=True)
    tiles = []
    for ground in ((214, 188, 140), (86, 112, 58)):
        g = np.ones((S, S, 3)) * np.array(ground, dtype=np.float64)
        g = g * (1 - out_a[..., None]) + out_rgb * out_a[..., None]
        g = g + glow[..., :3].astype(np.float64) * (glow_i[..., None])
        tiles.append(np.clip(g, 0, 255).astype(np.uint8))
    Image.fromarray(np.hstack(tiles)).save(os.path.join(MOD, "work", "nuke_target_preview.png"))
    print(os.path.join(art, "nuke_target.tga"), "+ glow")


if __name__ == "__main__":
    main()
