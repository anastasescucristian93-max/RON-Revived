"""French Large City 1-3 fixes on the finished sprite (2026-10-31):
- drops the dark wedge in the courtyard: Gemini's own shadow in the gap behind the right-hand
  houses passed the magenta key as building pixels;
- paints a player-colour band (alpha 248) along the first-floor beam of the big gabled house at
  the back, where the shared city had its band; Gemini lost it when the church went.

    python city_fix.py <in.png> <out.png> [preview.png]
"""
import sys

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

px = np.array(Image.open(sys.argv[1]).convert("RGBA"))
H, W = px.shape[:2]

# dark wedge: very dark building pixels inside the courtyard box, as one connected area
x0, y0, x1, y1 = 325, 195, 372, 232
lum = px[..., :3].astype(int).mean(-1)
dark = (px[..., 3] == 255) & (lum < 32)
box = np.zeros_like(dark)
box[y0:y1, x0:x1] = True
lab, n = ndimage.label(dark & box)
if n:
    big = np.argmax(np.bincount(lab.ravel())[1:]) + 1
    px[lab == big] = 0

# band: a 4 px line along the beam, on building pixels only
band = Image.new("L", (W, H))
ImageDraw.Draw(band).line([(221, 207), (282, 189)], fill=255, width=4)
band = (np.array(band) > 0) & (px[..., 3] == 255)
g = np.clip(lum * 95 // 70, 40, 140).astype(np.uint8)
px[band, 0] = px[band, 1] = px[band, 2] = g[band]
px[band, 3] = 248
Image.fromarray(px).save(sys.argv[2])

if len(sys.argv) > 3:
    view = px.copy()
    t = view[..., 3] == 248
    view[t, 0], view[t, 1], view[t, 2] = np.minimum(255, view[t, 0].astype(int) * 2), 30, 30
    view[view[..., 3] == 145] = (0, 0, 0, 110)
    view[view[..., 3] == 248, 3] = 255
    im = Image.fromarray(view)
    bg = Image.new("RGBA", im.size, (168, 156, 96, 255))
    bg.alpha_composite(im)
    bg.resize((W * 3, H * 3), Image.NEAREST).save(sys.argv[3])
print(f"{sys.argv[2]}: band {band.sum()} px")
