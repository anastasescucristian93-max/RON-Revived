r"""Cuts the ability icon (61 x 39, like the game's order icons) out of a picture.

    python make_icon.py <picture> <out.png> <left> <top> <width> [contrast] [saturation]

The box is <width> wide and 39/61 of that high. Contrast and saturation (default 1.25 and 1.2)
help a photo hold up at icon size. Also writes <out>_x4.png, enlarged for looking at.
"""
import sys

from PIL import Image, ImageEnhance

W, H = 61, 39


def icon(path, left, top, width, contrast=1.25, saturation=1.2):
    im = Image.open(path).convert("RGB")
    box = (left, top, left + width, top + round(width * H / W))
    im = im.crop(box).resize((W, H), Image.LANCZOS)
    im = ImageEnhance.Contrast(im).enhance(contrast)
    im = ImageEnhance.Color(im).enhance(saturation)
    return ImageEnhance.Sharpness(im).enhance(1.6)


if __name__ == "__main__":
    if len(sys.argv) < 6:
        sys.exit(__doc__)
    src, out = sys.argv[1], sys.argv[2]
    left, top, width = (int(x) for x in sys.argv[3:6])
    extra = [float(x) for x in sys.argv[6:8]]
    im = icon(src, left, top, width, *extra)
    im.save(out)
    im.resize((W * 4, H * 4), Image.NEAREST).save(out.replace(".png", "_x4.png"))
    print(out)
