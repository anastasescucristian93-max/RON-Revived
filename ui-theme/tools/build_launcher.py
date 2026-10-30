r"""Builds the "Revived" launcher theme into mod_override\launcher_res\ (read by the DLL in
patriots.exe, see camera-smooth\src\launcher.c).

    python build_launcher.py <game folder> <out mod_override folder>

Inputs: the elements cut by cut_sprites.py from source\launcher\concept.jpg
(ui-theme\elements\launcher), the logo with REVIVED (brand.py),
source\launcher\slides\*.(jpg|png) when present (the pictures the big panel fades through,
at random), otherwise source\launcher\illustration.(jpg|png) (one still picture; until
then the game's own fog is used, blurred), and the game's launcher_res, only read.

Outputs (under <out>\launcher_res):
  background.png, background_offline.png  the window with alpha (the window's shape)
  BUTTONU.bmp, BUTTOND.bmp                big button, normal / pressed
  IDB_MINIMIZE_U.bmp, IDB_CLOSE_U.bmp     window buttons
  layout.ini                              scale, button moves and the slideshow, for the DLL
  lang\<code>.png, lang\frame.png        the language button's flags and frame (lang_flags.py)
  slide_N.png, slide_under.png, slide_mask.png, slide_fog.png
                                          with slides: each picture at the panel's size, the
                                          panel behind them, the fade of their edges and the
                                          fog the DLL changes them through
The bitmaps have no alpha (the launcher blits them), so each is composed over the
background it sits on. The positions are the launcher's own (its dialog template), in the
game's 752x732 window; everything is drawn SCALE_PERCENT larger, and the DLL scales the
dialog's controls by the same amount (camera-smooth\src\launcher.c).
"""
import os
import sys

import numpy as np
from PIL import Image, ImageFilter, ImageOps

import brand
import lang_flags

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
ELEMENTS = os.path.join(ROOT, "elements", "launcher")

SCALE_PERCENT = 115            # the whole window, controls and captions included


def s(v):
    """A length of the game's 752x732 window at the theme's scale (rounded as the DLL does)."""
    return (v * SCALE_PERCENT + 50) // 100


W, H = s(752), s(732)
# Launcher controls, in background pixels (dialog units x 1.5 / 1.625, plus 16,27: the
# dialog is centred on the background, see camera-smooth\src\launcher.c).
BUTTONS = [(34, 266), (34, 321), (34, 376), (34, 432)]  # Play, Achievements, Website, Workshop
BUTTON_SIZE = (191, 34)        # as the dialog template places them
PLATE = (150, 40)              # the buttons' new size: the DLL resizes them to BUTTONU.bmp,
                               # centred on their place (camera-smooth\src\launcher.c)
PLAY_Y = 333                   # centre of Play, near the top of the button panel; the others
BUTTON_GAP = 54                # follow this far apart (14 px between plates), as one group
                               # (moved there by launcher_res\layout.ini)
MINIMIZE = (636, 56, 23, 23)   # the dialog template's sizes: the bitmaps are not
CLOSE = (666, 56, 24, 24)      # resized to, so they must match

COLUMN = (5, 10, 250)          # left column: x, y, width
LOGO_BOX_H = 220               # its top box (logo), then the button panel
PANEL = (262, 12, 745, 722)    # big panel: left, top, right, bottom
SLIDE_HOLD_MS = 3500           # each picture stays this long, then changes for another one,
SLIDE_FADE_MS = 2000           # at random, through fog, in this long
FOG_DRIFT = 140                # the fog drifts this far (theme pixels) during a change


def el(n):
    return Image.open(os.path.join(ELEMENTS, f"element_{n:02d}.png")).convert("RGBA")


def fit(img, w, h):
    return img.resize((w, h), Image.LANCZOS)


def nine_slice(img, w, h, c):
    """Stretches `img` to w x h keeping corners of c px (in img pixels) as they are."""
    iw, ih = img.size
    out = Image.new("RGBA", (w, h))
    xs = [(0, c, 0, c), (c, iw - c, c, w - c), (iw - c, iw, w - c, w)]
    ys = [(0, c, 0, c), (c, ih - c, c, h - c), (ih - c, ih, h - c, h)]
    for sx0, sx1, dx0, dx1 in xs:
        for sy0, sy1, dy0, dy1 in ys:
            part = img.crop((sx0, sy0, sx1, sy1)).resize((dx1 - dx0, dy1 - dy0), Image.LANCZOS)
            out.alpha_composite(part, (dx0, dy0))
    return out


def three_slice(img, w, h, cap):
    """Scales `img` to height h and width w, keeping caps of `cap` px (in img pixels)."""
    k = h / img.height
    cw = max(1, round(cap * k))
    left = img.crop((0, 0, cap, img.height)).resize((cw, h), Image.LANCZOS)
    right = img.crop((img.width - cap, 0, img.width, img.height)).resize((cw, h), Image.LANCZOS)
    mid = img.crop((cap, 0, img.width - cap, img.height)).resize((w - 2 * cw, h), Image.LANCZOS)
    out = Image.new("RGBA", (w, h))
    out.alpha_composite(left, (0, 0))
    out.alpha_composite(mid, (cw, 0))
    out.alpha_composite(right, (w - cw, 0))
    return out


def trim(img):
    return img.crop(img.getchannel("A").point(lambda a: 255 if a > 8 else 0).getbbox())


def slides(w, h):
    folder = os.path.join(ROOT, "source", "launcher", "slides")
    names = sorted(n for n in os.listdir(folder) if n.lower().endswith((".png", ".jpg", ".jpeg")))         if os.path.isdir(folder) else []
    return [ImageOps.fit(Image.open(os.path.join(folder, n)).convert("RGB"), (w, h), Image.LANCZOS)
            for n in names]


def noise(w, h, cells, seed):
    """Smooth fractal noise 0..1: random grids of `cells` and finer, upscaled and summed."""
    rng = np.random.default_rng(seed)
    out, amp, total = np.zeros((h, w)), 1.0, 0.0
    for c in cells:
        grid = Image.fromarray((rng.random((max(2, round(c * h / w)), c)) * 255).astype(np.uint8))
        out += amp * np.asarray(grid.resize((w, h), Image.BICUBIC), dtype=float) / 255
        total += amp
        amp *= 0.55
    out /= total
    return (out - out.min()) / (out.max() - out.min())


def fog(w, h):
    """The fog the slides change through: grey clouds like smoke; alpha is where it thickens
    first (low) and clears first (high), equalized so the fog spreads at an even pace."""
    shade = noise(w, h, (3, 6, 12, 24, 48), 7) ** 1.3
    colour = np.stack([150 + 85 * shade, 156 + 80 * shade, 164 + 72 * shade], axis=2)
    order = noise(w, h, (2, 4, 8, 16, 32), 11)
    order = np.argsort(np.argsort(order.ravel())).reshape(order.shape) / (order.size - 1)
    rgba = np.dstack([colour, order * 255]).clip(0, 255).astype(np.uint8)
    return Image.fromarray(rgba, "RGBA")


def illustration(game, w, h):
    for ext in ("png", "jpg", "jpeg"):
        p = os.path.join(ROOT, "source", "launcher", f"illustration.{ext}")
        if os.path.exists(p):
            return ImageOps.fit(Image.open(p).convert("RGB"), (w, h), Image.LANCZOS).convert("RGBA")
    # Placeholder: the game's own fog panel, blurred so its old logo and text disappear.
    old = Image.open(os.path.join(game, "launcher_res", "background_offline.png")).convert("RGB")
    fog = old.crop((310, 230, 660, 520)).filter(ImageFilter.GaussianBlur(14))
    return ImageOps.grayscale(fog).resize((w, h), Image.LANCZOS).convert("RGBA")


GRAIN_BASE = (23, 26, 31)      # the button panel's fill: dark steel blue,
GRAIN_GLOW = (36, 43, 56)      # a soft blue light from its top,
GRAIN_NOISE = 5.0              # and a fine grain (the user's "Granulatie" page background)


def grain_panel(lower):
    """The button panel with its fog replaced by a fine grain: the fog is found by its colour
    (bluish, smooth, dark) inside the frame, so the corner brackets and the frame stay."""
    a = np.asarray(lower, dtype=float)
    rgb, h, w = a[..., :3], a.shape[0], a.shape[1]
    lum = rgb.mean(axis=2)
    mean = np.asarray(Image.fromarray(lum.astype(np.uint8)).filter(ImageFilter.BoxBlur(2)), dtype=float)
    sq = np.asarray(Image.fromarray((lum ** 2 / 255).astype(np.uint8)).filter(ImageFilter.BoxBlur(2)),
                    dtype=float) * 255
    smooth = np.sqrt(np.maximum(sq - mean ** 2, 0)) < 6
    fog = (rgb[..., 2] - rgb[..., 0] >= 3) & smooth & (lum < 80) & (a[..., 3] > 250)
    inside = np.zeros_like(fog)
    inside[round(h * 0.058):round(h * 0.92), round(w * 0.115):round(w * 0.913)] = True
    m = Image.fromarray(((fog & inside) * 255).astype(np.uint8))
    for f in (ImageFilter.MinFilter(3), ImageFilter.MaxFilter(3), ImageFilter.MaxFilter(5),
              ImageFilter.MinFilter(5)):  # drop specks, fill the fog's own grain
        m = m.filter(f)
    m = np.asarray(m.filter(ImageFilter.GaussianBlur(0.7)), dtype=float) / 255
    # the fill: base, the light from the top centre, a shade along the frame, the grain
    yy, xx = np.mgrid[0:h, 0:w]
    glow = np.exp(-(((xx - w / 2) / (w * 0.55)) ** 2 + ((yy - h * 0.06) / (h * 0.30)) ** 2))
    fill = np.array(GRAIN_BASE) + (np.array(GRAIN_GLOW) - np.array(GRAIN_BASE)) * glow[..., None]
    edge = np.asarray(Image.fromarray((m * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(s(6))),
                      dtype=float) / 255
    fill *= (0.55 + 0.45 * np.clip((edge - 0.5) * 2.2, 0, 1))[..., None]
    rng = np.random.default_rng(3)
    fill += rng.normal(0, GRAIN_NOISE, (h, w))[..., None]  # grey grain, no coloured specks
    out = rgb * (1 - m[..., None]) + np.clip(fill, 0, 255) * m[..., None]
    return Image.fromarray(np.dstack([out, a[..., 3]]).clip(0, 255).astype(np.uint8), "RGBA")


def build_background(game, pictures):
    """The window; and the panel's picture area (left, top, right, bottom), what is behind the
    picture there and the picture's fade, for the slideshow."""
    bg = Image.new("RGBA", (W, H))

    # Big panel, 9-slice from the sheet's landscape frame, with the picture inside.
    l, t, r, b = (s(v) for v in PANEL)
    frame = el(2)
    k = 0.45 * SCALE_PERCENT / 100
    frame = frame.resize((round(frame.width * k), round(frame.height * k)), Image.LANCZOS)
    frame = nine_slice(frame, r - l, b - t, s(72))
    inner = (l + s(30), t + s(30), r - s(30), b - s(30))
    pw, ph = inner[2] - inner[0], inner[3] - inner[1]
    pic = pictures[0].convert("RGBA") if pictures else illustration(game, pw, ph)
    # Fade the picture's edges into the panel.
    mask = Image.new("L", pic.size, 0)
    mask.paste(255, (s(10), s(10), pic.width - s(10), pic.height - s(10)))
    mask = mask.filter(ImageFilter.GaussianBlur(s(8)))
    pic.putalpha(mask)
    bg.alpha_composite(frame, (l, t))
    behind = bg.crop(inner)
    bg.alpha_composite(pic, inner[:2])
    # The big logo, the WELCOME plate and the helmets are gone: the picture fills the panel
    # (the logo stays in the left column's box).

    # Left column: the logo box and the button panel, sized separately so the panel holds
    # the launcher's buttons.
    col = el(1).crop((0, 0, 545, 1485))
    split = 510
    x, y, cw = (s(v) for v in COLUMN)
    box_h = s(LOGO_BOX_H)
    top = fit(col.crop((0, 0, 545, split)), cw, box_h)
    lower = col.crop((0, split, 545, 1485))
    lower = fit(lower, cw, round(cw * lower.height / lower.width))
    bg.alpha_composite(grain_panel(lower), (x, y + box_h))
    bg.alpha_composite(top, (x, y))
    small = brand.lockup(s(128), revived_ratio=0.72)  # the frame's opening is about 160x140
    bg.alpha_composite(small, (x + (cw - small.width) // 2, y + (box_h - small.height) // 2))
    return bg, inner, behind, mask


def under(bg, x, y, w, h):
    return bg.crop((x, y, x + w, y + h))


def flat(img, bg_part):
    """The launcher blits 24-bit bitmaps: compose over what is behind them."""
    out = bg_part.copy()
    out.alpha_composite(img)
    return out.convert("RGB")


def keyed(img):
    """For a bitmap over the changing pictures: magenta where `img` is see-through, which the
    controls' dialog shows through (its colour key), so the pictures show around it."""
    out = Image.new("RGB", img.size, (255, 0, 255))
    out.paste(img.convert("RGB"), mask=img.getchannel("A").point(lambda a: 255 if a >= 128 else 0))
    return out


def centre_y(i):
    return PLAY_Y + i * BUTTON_GAP


def plate():
    return s(PLATE[0]), s(PLATE[1])


def placed(i):
    """Top-left of button i: plate() wide, centred on the template's column at centre_y(i),
    scaled as the DLL places it."""
    (x, _), bw, (pw, ph) = BUTTONS[i], BUTTON_SIZE[0], plate()
    return s(x + bw // 2) - pw // 2, s(centre_y(i)) - ph // 2


def layout_ini(n_slides, inner):
    """The window's scale, how far the DLL moves each button down from the template's
    place (in the game's 752x732 window, before scaling), and the slideshow: how many
    pictures and where, in background pixels."""
    lines = ["; written by ui-theme\\tools\\build_launcher.py, read by winmm.dll",
             "[window]", f"scale_percent={SCALE_PERCENT}", "[buttons]"]
    lines += [f"dy{i}={centre_y(i) - (y + BUTTON_SIZE[1] // 2)}" for i, (_, y) in enumerate(BUTTONS)]
    if n_slides > 1:
        l, t, r, b = inner
        lines += ["[slides]", f"count={n_slides}", f"x={l}", f"y={t}", f"w={r - l}", f"h={b - t}",
                  f"hold_ms={SLIDE_HOLD_MS}", f"fade_ms={SLIDE_FADE_MS}"]
    return "\r\n".join(lines) + "\r\n"


def main(game, out):
    res = os.path.join(out, "launcher_res")
    os.makedirs(res, exist_ok=True)
    l, t, r, b = (s(v) for v in PANEL)
    pictures = slides(r - l - 2 * s(30), b - t - 2 * s(30))
    bg, inner, behind_pic, mask = build_background(game, pictures)
    bg.save(os.path.join(res, "background.png"))
    bg.save(os.path.join(res, "background_offline.png"))
    for n in os.listdir(res):
        if n.startswith("slide_") and n.endswith(".png"):
            os.remove(os.path.join(res, n))
    if len(pictures) > 1:
        for i, pic in enumerate(pictures):
            pic.save(os.path.join(res, f"slide_{i + 1}.png"))
        behind_pic.save(os.path.join(res, "slide_under.png"))
        fade = Image.new("RGBA", mask.size, (255, 255, 255, 0))
        fade.putalpha(mask)
        fade.save(os.path.join(res, "slide_mask.png"))
        fog(mask.width + s(FOG_DRIFT), mask.height).save(os.path.join(res, "slide_fog.png"))

    pw, ph = plate()
    x, y = placed(0)
    behind = under(bg, x, y, pw, ph)
    with open(os.path.join(res, "layout.ini"), "w", newline="") as f:
        f.write(layout_ini(len(pictures), inner))
    flat(three_slice(trim(el(19)), pw, ph, 70), behind).save(os.path.join(res, "BUTTONU.bmp"))
    flat(three_slice(trim(el(15)), pw, ph, 90), behind).save(os.path.join(res, "BUTTOND.bmp"))
    for name, n, box in (("IDB_MINIMIZE_U", 3, MINIMIZE), ("IDB_CLOSE_U", 4, CLOSE)):
        bx, by, w, h = (s(v) for v in box)
        btn = fit(trim(el(n)), w, h)
        (keyed(btn) if len(pictures) > 1 else flat(btn, under(bg, bx, by, w, h))).save(
            os.path.join(res, f"{name}.bmp"))
    lang_flags.build(res)

    # Preview: the window as the launcher shows it.
    pv = bg.copy()
    for i in range(len(BUTTONS)):
        pv.paste(Image.open(os.path.join(res, "BUTTOND.bmp" if i == 0 else "BUTTONU.bmp")), placed(i))
    for name, box in (("IDB_MINIMIZE_U", MINIMIZE), ("IDB_CLOSE_U", CLOSE)):
        btn = Image.open(os.path.join(res, f"{name}.bmp")).convert("RGB")
        key = np.all(np.asarray(btn) == (255, 0, 255), axis=2)  # the dialog's colour key
        pv.paste(btn, (s(box[0]), s(box[1])), Image.fromarray(np.where(key, 0, 255).astype(np.uint8)))
    # the language button, closed and open (where launcher_lang.c puts it: under Workshop)
    k = SCALE_PERCENT / 100
    wx, wy = placed(len(BUTTONS) - 1)
    centre = (wx + pw / 2, wy + ph + (lang_flags.GAP + lang_flags.BUTTON / 2) * k)
    others = [c for c in ["EN"] + [c for c in lang_flags.CODES if c != "EN"] if c != "RO"]
    for name, picture in (("preview_launcher.png", lang_flags.preview(pv, centre, k, [], "RO")),
                          ("preview_language_button.png", lang_flags.preview(pv, centre, k, others, "RO"))):
        shown = Image.new("RGBA", pv.size, (90, 110, 90, 255))
        shown.alpha_composite(picture)
        shown.convert("RGB").save(os.path.join(ROOT, name))
    print(res)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
