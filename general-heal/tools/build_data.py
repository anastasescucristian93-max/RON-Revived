r"""Builds the General's Heal data files into mod_override\.

    python build_data.py <game folder> <out mod_override folder>

The Heal is the game's unfinished "Rally!" craft (type 0x279). The General was meant to have it:
its button place (4, 0) next to his four abilities, the "General Rally" animations, the
GENERAL_RALLY sound and the "General: Rally" hotkey are all in the game, but the cast does nothing.
This switches it on for the General (FROM General, PREQ0 None) at 1000 craft points, the same as
his other abilities, and renames it; winmm.dll (camera-smooth\src\heal.c) does the healing.
It also takes Create Decoys away from the General (DROPPED_CRAFT).

Files that other mods also change are built on their newest versions (MOD_ORDER), so build those
first; Romania builds on this one, so build it again afterwards and install it last.

The icon (icons\heal.png, cut from source\heal_gemini.jpg) goes in a free cell of the unit icon page
TEX_UNITS_2 (art\iface_units_03.tga, + _half), built on the Entrenchment's version, which holds the
Leave Fortification icon.

Writes Data\craftrules.xml, typenames.xml, help.xml, art\iface_units_03.tga, iface_units_03_half.tga.
"""
import os
import re
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
# mods whose mod_override\Data files are built on each other, newest first
MOD_ORDER = ["entrenchment", "paradrop", "data-center"]

CRAFT = "Rally!"
GRAPH = "RALLY"  # its help.xml entry
SHOWN = "Heal"
MANA = 1000
PERCENT, SECONDS = 25, 5  # as in ron_camera.ini [heal]
ATLAS, CELL = "iface_units_03.tga", (6, 10)  # empty since the Entrench button went away
ICON = os.path.join(HERE, "..", "icons", "heal.png")
FIELDS = {"FROM": "General", "PREQ0": "None", "MANA": str(MANA),
          "TEX_ID": "TEX_UNITS_2", "TEX_COL": str(CELL[0]), "TEX_ROW": str(CELL[1])}
HELP = (f"Heals your units near the General: over {SECONDS} seconds they get back {PERCENT}% of their "
        f"hit points. Squad members who already died do not return.")
# The General's Create Decoys is switched off (user, 2026-10-27): no unit has it (FROM None), and
# the General's help and tip 50 no longer mention it.
DROPPED_CRAFT = "Create Decoys"
DECOY_HELP = [
    (r"<BULLET/>\s*Can \{Create Decoy\} units that look and behave like real units to your enemies, "
     r"but cause no damage when they attack\.", ""),
    (r", and they can create \{Decoy\} units\s+to distract and confuse the enemy\.", "."),
]


def read(path):
    with open(path, encoding="latin-1", newline="") as fh:
        return fh.read()


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="latin-1", newline="") as fh:
        fh.write(text)
    print(path)


def base(game, name):
    for mod in MOD_ORDER:
        p = os.path.join(ROOT, mod, "mod_override", "Data", name)
        if os.path.exists(p):
            return p
    return os.path.join(game, "Data", name)


def set_craft(text, craft, fields):
    m = re.search(rf"<CRAFT>\s*<NAME>{re.escape(craft)}</NAME>.*?</CRAFT>", text, re.S)
    if not m:
        sys.exit(f"craftrules.xml: {craft} not found")
    block = m.group(0)
    for tag, value in fields.items():
        block, n = re.subn(rf"<{tag}>[^<]*</{tag}>|<{tag}/>", f"<{tag}>{value}</{tag}>", block)
        if n != 1:
            sys.exit(f"craftrules.xml: {craft} has {n} {tag}")
    return text[:m.start()] + block + text[m.end():]


def craft_rules(text):
    return set_craft(set_craft(text, CRAFT, FIELDS), DROPPED_CRAFT, {"FROM": "None"})


def type_names(text):
    text, n = re.subn(rf'(<STRING hash="\d+" needed="1">){re.escape(CRAFT)}(</STRING>)', rf"\g<1>{SHOWN}\g<2>", text)
    if n != 1:
        sys.exit(f"typenames.xml: {CRAFT} found {n} times")
    return text


def help_text(text):
    entry = f'<ENTRY name="{GRAPH}">\r\n\t\t\t<STRING>\r\n{HELP}\r\n</STRING>\r\n\t\t</ENTRY>'
    text, n = re.subn(rf'<ENTRY name="{GRAPH}">.*?</ENTRY>', lambda m: entry, text, count=1, flags=re.S)
    if n != 1:
        sys.exit(f"help.xml: {GRAPH} not found")
    for pattern, new in DECOY_HELP:
        text, n = re.subn(pattern, new, text)
        if n != 1:
            sys.exit(f"help.xml: decoy text found {n} times: {pattern}")
    return text


def icon(game, out):
    # 61 x 39 cells 62 x 40 apart; _half: 30 x 19, 31 x 20 apart
    for name, step, size in ((ATLAS, (62, 40), (61, 39)), (ATLAS.replace(".tga", "_half.tga"), (31, 20), (30, 19))):
        src = os.path.join(game, "art", name)
        for mod in MOD_ORDER:
            p = os.path.join(ROOT, mod, "mod_override", "art", name)
            if os.path.exists(p):
                src = p
                break
        atlas = Image.open(src).convert("RGBA")
        cell = Image.open(ICON).convert("RGBA").resize(size, Image.LANCZOS)
        atlas.paste(cell, (CELL[0] * step[0], CELL[1] * step[1]))
        path = os.path.join(out, "art", name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        atlas.save(path)
        print(path)


def main(game, out):
    data = os.path.join(out, "Data")
    write(os.path.join(data, "craftrules.xml"), craft_rules(read(base(game, "craftrules.xml"))))
    write(os.path.join(data, "typenames.xml"), type_names(read(base(game, "typenames.xml"))))
    write(os.path.join(data, "help.xml"), help_text(read(base(game, "help.xml"))))
    icon(game, out)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
