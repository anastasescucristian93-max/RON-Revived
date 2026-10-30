r"""Builds the web copy of the launcher for the player guide (site\RON_Revived), from the
launcher theme that build_launcher.py made.

    python build_launcher_web.py <launcher_res folder> <out folder>

The page (site\RON_Revived\index.html, section #look) shows background.png with the buttons
and the picture panel over it, and changes the pictures through fog as winmm.dll does in the
launcher (camera-smooth\src\launcher.c). The launcher draws its captions itself, with GDI+ in
Bahnschrift SemiBold Condensed; here they are drawn into the button images, so the page
looks the same on any computer.

Outputs: frame.png, button_<i>_up.png / button_<i>_down.png (Play, Achievements, RoN
Website, Workshop), lang\<code>.png and lang\frame.png (the language button's flags) with
lang\button_<i>_<code>_up/down.png (the captions in each language, from translations\launcher.json),
minimize.png, close.png, slide_<n>.jpg, slide_under.png, and the fade of
the pictures' edges and the fog as separate opaque images (slide_mask.png, fog_colour.png,
fog_order.png: a canvas loses colour where alpha is low, so no data is kept in alpha), and
layout.js (sizes and places, read by the page; a script, so the page also works from disk).
"""
import configparser
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

import build_launcher as bl
import lang_flags

CAPTIONS = ["Play", "Achievements", "RoN Website", "Workshop"]
FONT = r"C:\Windows\Fonts\bahnschrift.ttf"
FONT_STYLE = "SemiBold Condensed"
FONT_PX = 25.5                 # as the launcher shows it at 115% (measured on a screenshot)
CAP_TOP = 12.4                 # top of the capitals in the 173x46 button (same screenshot)
CAPTION_RGB = (222, 222, 222)
LAUNCHER_TEXTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "translations", "launcher.json")
CAPTION_KEYS = ["170023", "170027", "170025", "170024"]  # the launcher's ids of the four CAPTIONS
LANG_NAMES = ["Deutsch", "English", "Español", "Français", "Italiano", "Русский", "Português (Brasil)", "Polski",
              "Türkçe", "Română"]  # camera-smooth\src\lang.c, in lang_flags.CODES order


def caption(img, text):
    """`img` with `text` centred on it as the launcher centres it (x: the button's middle + 0.5)."""
    out = img.convert("RGBA")
    font = ImageFont.truetype(FONT, FONT_PX)
    font.set_variation_by_name(FONT_STYLE)
    cap_top = font.getbbox("H", anchor="ls")[1]         # negative: above the baseline
    left, _, right, _ = font.getbbox(text, anchor="ls")
    x = out.width / 2 + 0.5 - (left + right) / 2
    ImageDraw.Draw(out).text((x, CAP_TOP - cap_top), text, font=font, fill=CAPTION_RGB, anchor="ls")
    return out


def unkey(path):
    """A colour-keyed bitmap (magenta see-through) as RGBA."""
    rgb = np.asarray(Image.open(path).convert("RGB"))
    alpha = np.where(np.all(rgb == (255, 0, 255), axis=2), 0, 255).astype(np.uint8)
    return Image.fromarray(np.dstack([rgb, alpha]), "RGBA")


def main(res, out):
    os.makedirs(out, exist_ok=True)
    for n in os.listdir(out):
        if n.startswith("slide_"):
            os.remove(os.path.join(out, n))
    ini = configparser.ConfigParser()
    ini.read(os.path.join(res, "layout.ini"))
    sl = ini["slides"]
    frame = Image.open(os.path.join(res, "background.png"))
    frame.save(os.path.join(out, "frame.png"), optimize=True)

    up = Image.open(os.path.join(res, "BUTTONU.bmp"))
    down = Image.open(os.path.join(res, "BUTTOND.bmp"))
    buttons = []
    for i, text in enumerate(CAPTIONS):
        caption(up, text).convert("RGB").save(os.path.join(out, f"button_{i}_up.png"), optimize=True)
        caption(down, text).convert("RGB").save(os.path.join(out, f"button_{i}_down.png"), optimize=True)
        buttons.append(bl.placed(i))
    windows = {}
    for name, file, box in (("minimize", "IDB_MINIMIZE_U.bmp", bl.MINIMIZE), ("close", "IDB_CLOSE_U.bmp", bl.CLOSE)):
        img = unkey(os.path.join(res, file))
        img.save(os.path.join(out, f"{name}.png"), optimize=True)
        windows[name] = [bl.s(box[0]), bl.s(box[1]), img.width, img.height]

    # the language button (camera-smooth\src\launcher_lang.c): the flags, the frame and the
    # captions of the four buttons in every language, as the launcher shows them after a pick
    lang_out = os.path.join(out, "lang")
    os.makedirs(lang_out, exist_ok=True)
    texts = json.load(open(LAUNCHER_TEXTS, encoding="utf-8"))
    for code in lang_flags.CODES + ["frame"]:
        Image.open(os.path.join(res, "lang", f"{code}.png")).save(os.path.join(lang_out, f"{code}.png"), optimize=True)
    for code in lang_flags.CODES:
        for i, key in enumerate(CAPTION_KEYS):
            text = texts[code][key]
            caption(up, text).convert("RGB").save(os.path.join(lang_out, f"button_{i}_{code}_up.png"), optimize=True)
            caption(down, text).convert("RGB").save(os.path.join(lang_out, f"button_{i}_{code}_down.png"),
                                                     optimize=True)
    k = bl.SCALE_PERCENT / 100
    wx, wy = bl.placed(len(CAPTIONS) - 1)  # under Workshop
    pw, ph = bl.plate()
    lang = {
        "codes": lang_flags.CODES,
        "names": LANG_NAMES,
        "centre": [wx + pw / 2, wy + ph + (lang_flags.GAP + lang_flags.BUTTON / 2) * k],
        "k": k, "button": lang_flags.BUTTON, "petal": lang_flags.PETAL, "ring": lang_flags.RING,
        "open_ms": 460, "close_ms": 300, "delay": 0.05,
    }

    count = int(sl["count"])
    for n in range(1, count + 1):
        Image.open(os.path.join(res, f"slide_{n}.png")).convert("RGB").save(
            os.path.join(out, f"slide_{n}.jpg"), quality=90, optimize=True)
    for n in ("fog_colour.png", "fog_order.png"):
        if os.path.exists(os.path.join(out, n)):
            os.remove(os.path.join(out, n))
    Image.open(os.path.join(res, "slide_under.png")).convert("RGB").save(
        os.path.join(out, "slide_under.png"), optimize=True)
    Image.open(os.path.join(res, "slide_mask.png")).getchannel("A").save(
        os.path.join(out, "slide_mask.png"), optimize=True)
    fog = Image.open(os.path.join(res, "slide_fog.png"))
    fog.convert("RGB").save(os.path.join(out, "fog_colour.png"), optimize=True)
    fog.getchannel("A").save(os.path.join(out, "fog_order.png"), optimize=True)

    layout = {
        "size": list(frame.size),
        "panel": [int(sl["x"]), int(sl["y"]), int(sl["w"]), int(sl["h"])],
        "slides": count,
        "hold_ms": int(sl["hold_ms"]),
        "fade_ms": int(sl["fade_ms"]),
        "button": list(bl.plate()),
        "buttons": buttons,
        "windows": windows,
        "lang": lang,
    }
    with open(os.path.join(out, "layout.js"), "w") as f:
        f.write("/* written by ui-theme/tools/build_launcher_web.py */\nwindow.RON_LAUNCHER = "
                + json.dumps(layout) + ";\n")
    print(out, layout)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
