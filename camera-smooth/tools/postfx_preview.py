"""Before / after picture of the [graphics] look on a game screenshot.

    python postfx_preview.py <screenshot> <out.png> [--split] [--crop x y w h] [key=value ...]

The math is the same as the HLSL that postfx.c writes into ps_tonemap.hlsl (colour) and
ps_fxaa.hlsl (sharpening), so the picture shows what the game will draw, with two limits:
the screenshot already has the interface on it (in the game the interface is drawn after,
untouched), and a JPEG's own noise gets sharpened too.

Keys: sharpen, vibrance, contrast, warmth, black_level, vignette (defaults as in ron_camera.ini).
--split: one picture, left half as now, right half with the look (like Ctrl+F10 in the game).
Without it: the two pictures side by side.
"""
import sys

import numpy as np
from PIL import Image

LUMA = np.array([0.3, 0.59, 0.11], dtype=np.float32)
DEFAULTS = dict(sharpen=0.5, vibrance=0.15, contrast=0.12, warmth=0.0, black_level=0.02, vignette=0.12)


def grade(c, p):
    h, w, _ = c.shape
    c = np.clip((c - p["black_level"]) / (1.0 - p["black_level"]), 0, 1)
    c = c + p["contrast"] * (c * c * (3 - 2 * c) - c)
    sat = c.max(axis=2, keepdims=True) - c.min(axis=2, keepdims=True)
    l = (c * LUMA).sum(axis=2, keepdims=True)
    c = l + (c - l) * (1 + p["vibrance"] * (1 - sat))
    c = c * np.array([1 + p["warmth"], 1, 1 - p["warmth"]], dtype=np.float32)
    v = p["vignette"]
    if v > 0:
        y, x = np.mgrid[0:h, 0:w].astype(np.float32)
        d = (x / w - 0.5) ** 2 + (y / h - 0.5) ** 2
        t = np.clip((d - 0.1) / 0.4, 0, 1)
        c = c * (1 - v * (t * t * (3 - 2 * t)))[..., None]
    return np.clip(c, 0, 1)


def sharpen(e, src, s):
    if s <= 0:
        return e
    pad = np.pad(src, ((1, 1), (1, 1), (0, 0)), mode="edge")
    b, h_ = pad[:-2, 1:-1], pad[2:, 1:-1]
    d, f = pad[1:-1, :-2], pad[1:-1, 2:]
    mn = np.minimum(e, np.minimum(np.minimum(b, d), np.minimum(f, h_)))
    mx = np.maximum(e, np.maximum(np.maximum(b, d), np.maximum(f, h_)))
    amp = np.sqrt(np.clip(np.minimum(mn, 1 - mx) / np.maximum(mx, 1e-4), 0, 1))
    wgt = amp * (-1.0 / (8.0 + (5.0 - 8.0) * s))
    return np.clip((e + (b + d + f + h_) * wgt) / (1 + 4 * wgt), 0, 1)


def main():
    args = [a for a in sys.argv[1:]]
    split = "--split" in args
    crop = None
    if "--crop" in args:
        i = args.index("--crop")
        crop = tuple(int(v) for v in args[i + 1:i + 5])
        del args[i:i + 5]
    args = [a for a in args if a != "--split"]
    p = dict(DEFAULTS)
    for a in args[2:]:
        k, v = a.split("=")
        p[k] = float(v)
    img = np.asarray(Image.open(args[0]).convert("RGB"), dtype=np.float32) / 255.0
    # the game sharpens the tonemapped picture after FXAA: colour first, then sharpening
    out = sharpen(grade(img, p), grade(img, p), p["sharpen"])
    if crop:
        x, y, w, h = crop
        img, out = img[y:y + h, x:x + w], out[y:y + h, x:x + w]
    if split:
        res = img.copy()
        half = res.shape[1] // 2
        res[:, half:] = out[:, half:]
        res[:, half - 1:half + 1] = 1.0
    else:
        res = np.concatenate([img, np.ones((img.shape[0], 8, 3), np.float32), out], axis=1)
    Image.fromarray((res * 255 + 0.5).astype(np.uint8)).save(args[1])
    print("written", args[1], p)


if __name__ == "__main__":
    main()
