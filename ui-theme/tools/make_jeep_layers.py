r"""Splits the redrawn jeep scene into the menu's three parallax layers.

    python make_jeep_layers.py

Inputs (ui-theme\source\jeep_scene), three Gemini edits of one picture, so they line up:
  gemini_scene_willys.webp  the whole scene: the jeep bursting through the explosion
  gemini_no_jeep_small.webp the same with the jeep removed (the explosion filled in behind it)
  gemini_background.webp    the same with the explosion removed too (night, ruin, mud)
  jeep_matte.png            the jeep's matte from the whole scene (rmbg_matte.py)

Outputs (source\jeep_scene\hd, read by build_menu_theme.py):
  layer_0_background.png    the empty background, opaque
  layer_1_explosion.png     what the explosion adds to the background: the pixels brighter
                            than the background, with an alpha that rebuilds the no-jeep
                            picture over it (the dark debris stays the background's dark)
  layer_2_jeep.png          the jeep and its driver, cut with the matte
"""
import os

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "source", "jeep_scene")
OUT = os.path.join(SRC, "hd")
# Compression noise between the edits reaches about 20 levels: below NOISE the explosion
# layer is transparent, and it is opaque from NOISE + RAMP up.
NOISE, RAMP = 12.0, 40.0


def load(name):
    return np.asarray(Image.open(os.path.join(SRC, name)).convert("RGB"), np.float64)


def save(rgb, alpha, name):
    os.makedirs(OUT, exist_ok=True)
    a = np.clip(alpha, 0, 1)[..., None] * 255
    img = Image.fromarray(np.dstack([np.clip(rgb, 0, 255), a]).round().astype(np.uint8), "RGBA")
    img.save(os.path.join(OUT, name))
    print(os.path.join(OUT, name), img.size)


def main():
    scene = load("gemini_scene_willys.webp")
    no_jeep = load("gemini_no_jeep_small.webp")
    bg = load("gemini_background.webp")
    assert scene.shape == no_jeep.shape == bg.shape, (scene.shape, no_jeep.shape, bg.shape)

    save(bg, np.ones(bg.shape[:2]), "layer_0_background.png")

    # Explosion: alpha from how much brighter than the background a pixel is; the colour is
    # solved so that alpha * colour + (1 - alpha) * background gives the no-jeep picture.
    d = (no_jeep - bg).max(axis=2)
    alpha = np.clip((d - NOISE) / RAMP, 0, 1)
    alpha = ndimage.gaussian_filter(alpha, 1.0)
    safe = np.maximum(alpha, 1e-3)[..., None]
    color = np.where(alpha[..., None] > 0.02, bg + (no_jeep - bg) / safe, no_jeep)
    save(color, alpha, "layer_1_explosion.png")

    # Jeep: the matte, without the specks it picked up away from the jeep (largest part only).
    matte = np.asarray(Image.open(os.path.join(SRC, "jeep_matte.png")).convert("L"), np.float64) / 255
    labels, n = ndimage.label(matte > 0.5)
    sizes = ndimage.sum(np.ones_like(matte), labels, range(1, n + 1))
    keep = labels == 1 + int(np.argmax(sizes))
    keep = ndimage.binary_dilation(keep, iterations=6)
    save(scene, matte * keep, "layer_2_jeep.png")


if __name__ == "__main__":
    main()
