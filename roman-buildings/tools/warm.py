import sys
from PIL import Image, ImageEnhance
src, out, gamma = sys.argv[1], sys.argv[2], float(sys.argv[3])
im = Image.open(src).convert("RGB")
r, g, b = im.split()
def lut(k):  # gamma lift then channel gain; keeps 0 at 0 so magenta stays magenta
    return [min(255, round(255 * (i / 255) ** gamma * k)) for i in range(256)]
r, g, b = r.point(lut(1.06)), g.point(lut(1.0)), b.point(lut(0.86))
im = Image.merge("RGB", (r, g, b))
im = ImageEnhance.Color(im).enhance(1.15)
# put the key colour back exactly where the source was magenta
src_px, o = Image.open(src).convert("RGB").load(), im.load()
W, H = im.size
for y in range(H):
    for x in range(W):
        p = src_px[x, y]
        if p[0] > 200 and p[2] > 200 and p[1] < 60:
            o[x, y] = p
im.save(out)
