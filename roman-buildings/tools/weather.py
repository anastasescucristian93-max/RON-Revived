"""Adds wear to a finished building sprite: AI images come out too clean next to the game's own
art, which has grime, dark wall bases and deep crevices.

    python weather.py <sprite.tga|png> <out.png> [strength=1.0] [seed=1]

Only building pixels (alpha 255) change; player colour (248), shadow (145) and empty pixels stay
as they are. Then write the .tga with write_like.py. Strength 0.5 is light, 1 medium, 1.5 heavy.
"""
import sys

import numpy as np
from PIL import Image, ImageFilter

SOLID = 255
BASE_DIRT = 9  # pixels: how far up from the ground the wall bases get darker


def noise(h, w, cell, rng, cell_y=None):
    """Smooth value noise in 0..1 with features about `cell` pixels wide (`cell_y` tall)."""
    cy = cell_y or cell
    g = rng.random((h // cy + 2, w // cell + 2)).astype(np.float32)
    im = Image.fromarray((g * 255).astype(np.uint8)).resize(
        ((w // cell + 2) * cell, (h // cy + 2) * cy), Image.BICUBIC)
    return np.asarray(im, np.float32)[:h, :w] / 255


def base_distance(solid):
    """Per pixel: how many building pixels lie below it in its column before the edge."""
    h, w = solid.shape
    d = np.zeros((h, w), np.int32)
    run = np.zeros(w, np.int32)
    for y in range(h - 1, -1, -1):
        run = np.where(solid[y], run + 1, 0)
        d[y] = run
    return d


def weather(img, strength, seed):
    a = np.array(img.convert("RGBA"))
    rgb = a[..., :3].astype(np.float32)
    solid = a[..., 3] == SOLID
    h, w = solid.shape
    rng = np.random.default_rng(seed)

    # grime: blotches at two scales, darker and browner where the noise is high
    grime = 0.6 * noise(h, w, 18, rng) + 0.4 * noise(h, w, 5, rng)
    grime = np.clip((grime - 0.4) * 2.2, 0, 1) * 0.3 * strength
    dirt = np.array([0.93, 0.88, 0.80], np.float32)  # brown cast of dirt
    rgb *= 1 - grime[..., None] * (1 - dirt * 0.8)
    rgb *= (1 - grime * 0.5)[..., None]

    # smooth areas (plaster, planks) carry most of the wear: the game's own art is never flat
    lum = rgb @ np.array([0.3, 0.59, 0.11], np.float32)
    l8 = Image.fromarray(np.clip(lum, 0, 255).astype(np.uint8))
    mean = np.asarray(l8.filter(ImageFilter.BoxBlur(2)), np.float32)
    var = np.asarray(Image.fromarray(np.clip((lum - mean) ** 2, 0, 255).astype(np.uint8)).filter(
        ImageFilter.BoxBlur(2)), np.float32)
    flat = np.clip(1 - var / 40, 0, 1)
    bright = np.clip((lum - 90) / 120, 0, 1)

    # rain streaks running down the walls
    streak = np.clip((noise(h, w, 2, rng, 16) - 0.5) * 2.5, 0, 1)
    rgb *= 1 - (streak * (0.10 + 0.18 * flat * bright) * strength)[..., None]

    # fine grain: chipped paint, grain of wood and stone
    grain = 0.5 * (noise(h, w, 1, rng) + noise(h, w, 2, rng)) - 0.5
    rgb *= 1 + (grain * (0.10 + 0.22 * flat) * strength)[..., None]

    # wall bases: mud and damp near the ground
    d = base_distance(solid)
    base = np.clip(1 - (d - 1) / BASE_DIRT, 0, 1) * 0.28 * strength
    rgb *= 1 - base[..., None] * (1 - dirt * 0.6)

    # crevices: push pixels darker than their surroundings further down (local contrast)
    lum = rgb @ np.array([0.3, 0.59, 0.11], np.float32)
    blur = np.asarray(Image.fromarray(np.clip(lum, 0, 255).astype(np.uint8)).filter(
        ImageFilter.GaussianBlur(2)), np.float32)
    deep = np.clip(blur - lum, 0, 60) / 60 * 0.35 * strength
    rgb *= (1 - deep)[..., None]

    # slightly duller colours
    lum = rgb @ np.array([0.3, 0.59, 0.11], np.float32)
    rgb = rgb + (lum[..., None] - rgb) * 0.12 * strength

    out = a.copy()
    out[..., :3] = np.where(solid[..., None], np.clip(rgb, 0, 255), a[..., :3]).astype(np.uint8)
    return Image.fromarray(out)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    strength = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
    seed = int(sys.argv[4]) if len(sys.argv) > 4 else 1
    weather(Image.open(src), strength, seed).save(dst)
    print(f"{dst}: wear {strength}")


if __name__ == "__main__":
    main()
