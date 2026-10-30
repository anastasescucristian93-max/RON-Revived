"""Scales a finished building sprite around a point, keeping the canvas, for art that came out
too big for its footprint (make_sprite fits the new art to the original's height, so a picture
with bigger, more spread-out houses overflows the building's diamond on the ground).

    python rescale_sprite.py <sprite.tga|png> <out.png> <scale> <cx> <cy> [dx dy]

(cx, cy) stays in place: use the centre of the footprint diamond (footprint_check.py prints it).
(dx, dy) then moves the art, in pixels, when it sits off-centre on its footprint
(footprint_check.py --shift <texture> <sprite> finds a move).
The alpha categories (building, player colour, shadow) are scaled as masks, so edges stay hard.
Then add wear with weather.py and write the .tga with write_like.py.
"""
import sys

import numpy as np
from PIL import Image

from make_sprite import SHADOW, SOLID, TEAM


def scaled(img, s, cx, cy, resample, dx=0, dy=0):
    """img scaled by s around (cx, cy) and moved by (dx, dy), on a canvas of the same size."""
    W, H = img.size
    w, h = max(1, round(W * s)), max(1, round(H * s))
    out = Image.new(img.mode, (W, H))
    out.paste(img.resize((w, h), resample), (round(cx - cx * s + dx), round(cy - cy * s + dy)))
    return out


def main(src, dst, s, cx, cy, dx=0, dy=0):
    a = np.array(Image.open(src).convert("RGBA"))
    cat = a[..., 3]
    body = (cat == SOLID) | (cat == TEAM)
    masks = {}
    for name, m in (("body", body), ("team", cat == TEAM), ("shadow", cat == SHADOW)):
        im = Image.fromarray((m * 255).astype(np.uint8))
        masks[name] = np.array(scaled(im, s, cx, cy, Image.LANCZOS, dx, dy)) >= 128
    # colour: premultiply by the building mask, scale, divide, so edges keep the building's colour
    rgb = a[..., :3].astype(float) * body[..., None]
    pre = np.stack([np.array(scaled(Image.fromarray(rgb[..., i].astype(np.uint8)), s, cx, cy, Image.LANCZOS, dx, dy))
                    for i in range(3)], -1).astype(float)
    cov = np.array(scaled(Image.fromarray((body * 255).astype(np.uint8)), s, cx, cy, Image.LANCZOS, dx, dy)) / 255.0
    col = np.clip(pre / np.maximum(cov, 1e-3)[..., None], 0, 255).astype(np.uint8)
    out = np.zeros_like(a)
    sh = masks["shadow"] & ~masks["body"]
    out[sh, 3] = SHADOW
    out[masks["body"], :3] = col[masks["body"]]
    out[masks["body"], 3] = SOLID
    team = masks["body"] & masks["team"]
    out[team, 3] = TEAM
    Image.fromarray(out).save(dst)
    print(f"{dst}: scale {s} around ({cx}, {cy}), moved ({dx}, {dy}); building {int(masks['body'].sum())} px, "
          f"player colour {int(team.sum())} px")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], *(float(v) for v in sys.argv[3:8]))
