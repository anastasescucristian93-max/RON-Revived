r"""The "Rise of Nations / REVIVED" logo shared by the menu and launcher builders.

Sources, drawn on magenta (keyed out like cut_sprites.py):
  source\logo\logo_ron.jpg          the Rise of Nations logo (gear and globe)
  source\logo\revived_variants.jpg  nine "REVIVED" lettering styles in a 3x3 grid; style 6
                                    (gunmetal, bevelled, side lines) is the one used
"""
import os

import numpy as np
from PIL import Image

from cut_sprites import key_out

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "..", "source", "logo")
REVIVED_CROP = (1800, 640, 2752, 910)  # style 6 in the 2752x1536 sheet, below its "6" label


def available():
    return all(os.path.exists(os.path.join(SOURCE, f)) for f in ("logo_ron.jpg", "revived_variants.jpg"))


def _keyed(path, box=None):
    img = Image.open(path).convert("RGB")
    if box:
        img = img.crop(box)
    color, alpha = key_out(np.asarray(img, dtype=np.float64) / 255.0)
    alpha = np.where(alpha < 0.06, 0.0, alpha)  # JPEG noise in the background
    out = Image.fromarray((np.dstack([color, alpha]) * 255 + 0.5).astype(np.uint8), "RGBA")
    return out.crop(out.getchannel("A").point(lambda a: 255 if a > 40 else 0).getbbox())


def parts():
    """(logo, revived) as trimmed RGBA images at source resolution."""
    logo = _keyed(os.path.join(SOURCE, "logo_ron.jpg"))
    revived = _keyed(os.path.join(SOURCE, "revived_variants.jpg"), REVIVED_CROP)
    return logo, revived


def lockup(width, revived_ratio=0.8, overlap=0.0):
    """The logo with REVIVED centred below it, `width` px wide; REVIVED is revived_ratio of
    the width, starts `overlap` x width above the logo's bottom and is drawn in front of the
    gear, which reaches below the title."""
    logo, revived = parts()
    logo = logo.resize((width, round(width * logo.height / logo.width)), Image.LANCZOS)
    rw = round(width * revived_ratio)
    revived = revived.resize((rw, round(rw * revived.height / revived.width)), Image.LANCZOS)
    y = logo.height - round(width * overlap)
    out = Image.new("RGBA", (width, y + revived.height))
    out.alpha_composite(logo, (0, 0))
    out.alpha_composite(revived, ((width - rw) // 2, y))
    return out
