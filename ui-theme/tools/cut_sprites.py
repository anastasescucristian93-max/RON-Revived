"""Cuts interface elements out of a sheet drawn on a magenta background.

    python cut_sprites.py <sheet.png|jpg> <out_dir> [merge_px]

- Magenta (255, 0, 255) becomes transparent with a soft edge: alpha follows how magenta a
  pixel is, so anti-aliased and JPEG-blurred edges keep a smooth outline.
- Edge pixels are "unmixed" from the magenta (despill), so no pink halo is left.
- Every separate element is saved as its own PNG (element_01.png, ...), numbered left to
  right, top to bottom. Parts closer than `merge_px` (default 4) are kept together, so a
  frame and its screws stay one element.
- preview.png shows all of them on a checkerboard, with their numbers.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

KEY_SOLID = 0.80  # magenta strength at which a pixel is fully background
KEY_EDGE = 0.30   # below this it is fully opaque
MIN_AREA = 30     # smaller specks (JPEG noise) are dropped


def key_out(rgb):
    """RGB floats 0..1 -> premultiplied-free RGB and alpha with magenta removed."""
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    strength = np.clip(np.minimum(r, b) - g, 0, 1)  # 1 = pure magenta, 0 = grey/neutral
    alpha = np.clip((KEY_SOLID - strength) / (KEY_SOLID - KEY_EDGE), 0, 1)
    # Despill: a semi-transparent pixel is colour*a + magenta*(1-a); take the magenta out.
    a = np.maximum(alpha, 1e-3)[..., None]
    magenta = np.array([1.0, 0.0, 1.0])
    color = np.clip((rgb - (1 - a) * magenta) / a, 0, 1)
    # Magenta cast bled into the element itself (JPEG chroma blur makes dark edges purple):
    # wherever both red and blue exceed green, take the common excess out of both. Neutral
    # metal becomes neutral again; real reds and blues keep their hue.
    spill = np.clip(np.minimum(color[..., 0], color[..., 2]) - color[..., 1], 0, 1)[..., None]
    color = np.clip(color - spill * np.array([1.0, 0.0, 1.0]), 0, 1)
    # The JPEG background is not exactly (255, 0, 255), so unmixing over-corrects the soft
    # edge (red and blue clip to 0 and it turns green). Edge pixels take the colour of the
    # nearest solid pixel instead; only their alpha comes from the key.
    solid = alpha >= 0.95
    if solid.any():
        _, (iy, ix) = ndimage.distance_transform_edt(~solid, return_indices=True)
        color = np.where(solid[..., None], color, color[iy, ix])
    return color, alpha


def main(src, out_dir, merge_px=4):
    os.makedirs(out_dir, exist_ok=True)
    rgb = np.asarray(Image.open(src).convert("RGB"), dtype=np.float64) / 255.0
    color, alpha = key_out(rgb)

    mask = alpha > 0.08
    grown = ndimage.binary_dilation(mask, iterations=merge_px) if merge_px > 0 else mask
    labels, n = ndimage.label(grown)
    boxes = ndimage.find_objects(labels)

    items = []
    for i, sl in enumerate(boxes):
        part = (labels[sl] == i + 1) & mask[sl]
        if part.sum() < MIN_AREA or alpha[sl][part].max() < 0.5:
            continue  # JPEG specks and faint leftovers with no solid pixel
        ys, xs = np.nonzero(part)
        y0, y1 = sl[0].start + ys.min(), sl[0].start + ys.max() + 1
        x0, x1 = sl[1].start + xs.min(), sl[1].start + xs.max() + 1
        items.append((y0, x0, y1, x1, i + 1))
    # Reading order: rows of roughly the same top, then left to right.
    items.sort(key=lambda t: (t[0] // 40, t[1]))

    rgba = np.dstack([color, alpha])
    pad = 2
    h, w = alpha.shape
    saved = []
    for k, (y0, x0, y1, x1, lab) in enumerate(items, 1):
        y0p, x0p, y1p, x1p = max(0, y0 - pad), max(0, x0 - pad), min(h, y1 + pad), min(w, x1 + pad)
        crop = rgba[y0p:y1p, x0p:x1p].copy()
        own = labels[y0p:y1p, x0p:x1p] == lab  # only this element, not a neighbour's edge
        crop[..., 3] *= own
        img = Image.fromarray((crop * 255 + 0.5).astype(np.uint8), "RGBA")
        name = f"element_{k:02d}.png"
        img.save(os.path.join(out_dir, name))
        saved.append((name, x0p, y0p, img))
        print(f"{name}: {img.width}x{img.height} at {x0p},{y0p}")

    # Preview on a checkerboard, with numbers.
    cell = 8
    board = Image.new("RGBA", (w, h))
    px = ImageDraw.Draw(board)
    for yy in range(0, h, cell):
        for xx in range(0, w, cell):
            c = 200 if (xx // cell + yy // cell) % 2 else 150
            px.rectangle([xx, yy, xx + cell - 1, yy + cell - 1], fill=(c, c, c, 255))
    for name, x0, y0, img in saved:
        board.alpha_composite(img, (x0, y0))
    draw = ImageDraw.Draw(board)
    for k, (name, x0, y0, img) in enumerate(saved, 1):
        draw.rectangle([x0, y0, x0 + img.width - 1, y0 + img.height - 1], outline=(0, 160, 255, 255))
        draw.text((x0 + 2, y0 + 1), str(k), fill=(0, 90, 255, 255))
    board.save(os.path.join(out_dir, "preview.png"))
    print(f"{len(saved)} elements, preview.png written")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 4)
