"""Removes the colour cast a magenta background leaves on AI-made art.

Gemini lights the building as if the magenta backdrop were real, so stone, concrete and
glass come out pinkish. The game's own art has warm neutrals (blue below green). This
scales the red and blue channels of the building so that its near-neutral pixels get the
same balance as the original sprite's.

    python color_match.py <sprite.tga> <original.tga> [out.tga]

Only building pixels (alpha 255) change; player colour (248) and shadow (145) stay.
Running it twice changes nothing more.
"""
import sys

import numpy as np
from PIL import Image

SOLID = 255


def neutral_balance(rgba):
    """Mean R-G and B-G over near-grey, mid-bright building pixels, and their mean RGB."""
    a = rgba.astype(np.int32)
    rgb = a[a[..., 3] == SOLID][:, :3]
    spread = rgb.max(1) - rgb.min(1)
    lum = rgb.mean(1)
    n = rgb[(spread < 45) & (lum > 60) & (lum < 235)]
    return n.mean(0)


def match(sprite, original):
    """sprite, original: RGBA arrays. Returns the corrected sprite and the gains used."""
    r, g, b = neutral_balance(sprite)
    orr, og, ob = neutral_balance(original)
    gr = (g + (orr - og)) / r
    gb = (g + (ob - og)) / b
    out = sprite.copy()
    m = sprite[..., 3] == SOLID
    px = out[m].astype(np.float32)
    px[:, 0] *= gr
    px[:, 2] *= gb
    out[m] = np.clip(np.round(px), 0, 255).astype(np.uint8)
    return out, (gr, gb)


def main(sprite_path, original_path, out_path=None):
    sprite = np.array(Image.open(sprite_path).convert("RGBA"))
    original = np.array(Image.open(original_path).convert("RGBA"))
    out, (gr, gb) = match(sprite, original)
    Image.fromarray(out).save(out_path or sprite_path)
    print(f"{sprite_path}: red x{gr:.3f}, blue x{gb:.3f}")


if __name__ == "__main__":
    main(*sys.argv[1:4])
