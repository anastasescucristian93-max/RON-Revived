r"""Build icons for the three city districts, from their Gemini pictures: the building keyed off its
white background and stood on a painted ground under a sky, in the icon page's cell shape
(61 x 39, drawn here at 8x). build_data.py scales them into the icon atlas.

    python district_icons.py

Writes icons\district_housing.png, district_commerce.png, district_industry.png.
"""
import os

import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
GEM = os.path.join(HERE, "..", "gemini")
OUT = os.path.join(HERE, "..", "icons")
W, H = 488, 312
# per kind: sky colour (top, bottom), like the old Expansions' icons (population green, commerce gold, efficiency blue)
KINDS = {
    "housing": ((70, 120, 95), (170, 205, 175)),
    "commerce": ((120, 95, 45), (225, 200, 140)),
    "industry": ((50, 75, 110), (160, 185, 210)),
}


def key(path):
    a = np.asarray(Image.open(path).convert("RGB")).astype(int)
    bg = (a.min(2) > 222) & ((a.max(2) - a.min(2)) < 16)
    lab, _ = ndimage.label(bg)
    border = np.unique(np.r_[lab[0], lab[-1], lab[:, 0], lab[:, -1]])
    body = ~np.isin(lab, border[border > 0])
    body = ndimage.binary_erosion(body, np.ones((3, 3)))
    rgba = Image.fromarray(np.dstack([a, body * 255]).astype(np.uint8), "RGBA")
    return rgba.crop(rgba.getbbox())


def background(top, bottom):
    y = np.linspace(0, 1, H)[:, None, None]
    sky = np.array(top) * (1 - y) + np.array(bottom) * y
    img = np.repeat(sky, W, axis=1)
    ground = np.array([96, 92, 80.0])
    gy = int(H * 0.78)
    noise = np.random.default_rng(1).normal(0, 6, (H - gy, W, 1))
    img[gy:] = ground + noise
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1)).convert("RGBA")


def main():
    os.makedirs(OUT, exist_ok=True)
    for kind, (top, bottom) in KINDS.items():
        b = key(os.path.join(GEM, "district_%s_v1.jpg" % kind))
        k = min(W * 0.62 / b.width, H * 0.92 / b.height)
        b = b.resize((int(b.width * k), int(b.height * k)), Image.LANCZOS)
        icon = background(top, bottom)
        icon.alpha_composite(b, ((W - b.width) // 2, H - b.height - int(H * 0.04)))
        out = os.path.join(OUT, "district_%s.png" % kind)
        icon.convert("RGB").save(out)
        print(out)


if __name__ == "__main__":
    main()
