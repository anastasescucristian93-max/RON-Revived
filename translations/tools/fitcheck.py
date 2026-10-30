r"""Finds translations that may not fit their box on screen.

    python fitcheck.py <code> [<code> ...]       writes work\<code>\fit.json and prints a summary

Every short text (one line) is measured in the game's own font (Data\Fonts) and compared with a
budget:
- the DLL's panel texts: the real box (DLL_BOXES, from camera-smooth\src\assist_panel.c) in the
  font and size the panel draws them with;
- the game's texts that have the game's German / Spanish / French / Italian: the widest of the
  English and those four (the box fits the widest, or the game would show it cut), plus 8 %;
- our mods' other texts: the English width plus 35 %.
fit.json lists {"id", "chunk", "en", "text", "width", "budget"} for the texts over budget, for a
translator to shorten (same rules, TRANSLATING.md).
"""
import json
import os
import re
import sys

from PIL import ImageFont

import langs as L
import merge
import xmltext as X

FONTS = os.path.join(L.GAME, "Data", "Fonts")
UI = ("arialbd.ttf", 12)  # most labels and buttons: the game's Arial Bold 12
# DLL text -> (box width in pixels, font file, size); assist_panel.c: PW 320, the switch 288 wide
DLL_BOXES = {
    "AI ASSISTANT": (300, "georgiab.ttf", 14),
    "NO CONTROL": (136, "arialbd.ttf", 12),
    "CUSTOM": (136, "arialbd.ttf", 12),
    "FULL CONTROL": (136, "arialbd.ttf", 12),
    "Only in single player games": (300, "arial.ttf", 12),
    "The computer plays your nation": (300, "arial.ttf", 12),
    "The computer plays, only what is ticked": (300, "arial.ttf", 12),
    "You play your nation": (300, "arial.ttf", 12),
    "THE COMPUTER MAY BUY": (280, "arialbd.ttf", 12),
    "Keep at least %d of each resource": (280, "arial.ttf", 12),
    "Economy": (244, "arial.ttf", 12),
    "Military": (244, "arial.ttf", 12),
    "Research": (244, "arial.ttf", 12),
    "New cities": (244, "arial.ttf", 12),
    "Wonders and Data Center": (244, "arial.ttf", 12),
    "PLAY": (150, "georgiab.ttf", 22),
}
_fonts = {}


def font(name, size):
    if (name, size) not in _fonts:
        _fonts[name, size] = ImageFont.truetype(os.path.join(FONTS, name), size)
    return _fonts[name, size]


def shown(xml):
    """the text as the game shows it, with sample values for the tokens"""
    s = X.plain(xml)
    s = re.sub(r"\$[A-Z_]+\d*", "100", s)
    s = re.sub(r"#ICON\d+", "  ", s)
    s = s.replace("%%", "%").replace("%d", "1000").replace("%s", "Sergeant")
    return s.replace("{", "").replace("}", "")


def width(xml, face=UI):
    return font(*face).getlength(shown(xml))


def one_line(xml):
    return "\n" not in X.plain(xml).strip() and "<BULLET" not in xml


def name_widths(code, share=0.95):
    """the width 95 % of the game's names in typenames.xml stay within, in this language"""
    if code not in _names:
        if code in L.GAME_LANGS:
            units = X.units(X.load(L.game_file("typenames.xml", L.GAME_LANGS[code][0])))
            names = [X.inner(e).strip() for e in units.values()]
        else:
            with open(os.path.join(L.WORK, "glossary", code + ".json"), encoding="utf-8") as f:
                names = list(json.load(f).values())
        ws = sorted(width(n) for n in names)
        _names[code] = ws[int(len(ws) * share) - 1]
    return _names[code]


_names = {}


def check(code):
    p = os.path.join(L.TEXT, code + ".json")
    if not os.path.exists(p):
        print(f"{code}: no translations yet")
        return
    with open(p, encoding="utf-8") as f:
        store = json.load(f)
    over, checked = [], 0
    chunk_dir = os.path.join(L.WORK, "chunks")
    for cf in sorted(os.listdir(chunk_dir)):
        with open(os.path.join(chunk_dir, cf), encoding="utf-8") as f:
            items = json.load(f)
        for it in items:
            en, text = it["en"], store.get(merge.key_of(it))
            if text is None or not one_line(en):
                continue
            if it["file"] == "DLL":
                if en not in DLL_BOXES:
                    continue  # tooltip lines wrap
                box, face, size = DLL_BOXES[en]
                w, budget = width(text, (face, size)), box
            elif it["file"] == "typenames.xml" and ("ref" not in it or "was" in it):
                # a new name of our mods: where names are shown, the game's long names fit
                w, budget = width(text), max(width(en) * 1.35, name_widths(code))
            elif it.get("ref") and "was" not in it:
                w = width(text)
                budget = max([width(en)] + [width(r) for r in it["ref"].values() if one_line(r)]) * 1.08
            elif len(X.plain(en).split()) <= 12:
                w, budget = width(text), width(en) * 1.35
            else:
                continue  # long texts wrap
            checked += 1
            if w > budget + 2:
                over.append({"id": it["id"], "chunk": cf[:-5], "en": en, "text": text,
                             "width": round(w), "budget": round(budget)})
    os.makedirs(os.path.join(L.WORK, code), exist_ok=True)
    with open(os.path.join(L.WORK, code, "fit.json"), "w", encoding="utf-8") as f:
        json.dump(over, f, ensure_ascii=False, indent=1)
    print(f"{code}: {checked} short texts measured, {len(over)} over budget (work\\{code}\\fit.json)")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for c in sys.argv[1:]:
        check(c)
