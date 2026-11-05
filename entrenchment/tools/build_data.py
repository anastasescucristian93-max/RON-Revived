r"""Builds the Entrenchment mod's data files into mod_override\.

    python build_data.py <game folder> <out mod_override folder>

The game refuses extra building types, so the Field Fortification (the "Nest" in the code) takes the slot of the scenario-only
Lenin Statue (type 0x1C2, building 36), as the Data Center took the Washington Monument's:
same internal name, new rules, graphics, shown name, help and icon. Ranged infantry build it with
the game's own build button, which winmm.dll puts on their panel; it leaves it out of the Citizens'
build menu and out of the computer players' plans. Its garrison lets the game's own garrison
command send infantry to it;
the mod's winmm.dll (camera-smooth\src\entrench.c) keeps them outside, dug in, at their places in
the ring.

Files that the Data Center and Airdrop mods also change are built on top of their versions
when those are there (buildingrules, building_graphics and the wonders' icon page from data-center\mod_override\,
typenames and help from paradrop\mod_override\, which already holds the Data Center's), so
every mod's changes are in them. Build those mods first.

The Leave Fortification button is an unused untargeted craft (CRAFTS), switched on in
craftrules.xml (built on the Airdrop's), renamed, with help and icons\exit.png in a free cell of
art\iface_units_03.tga (+ _half). Getting in uses the game's own
garrison command (right-click, or its garrison button and hotkey).

The Tank Traps (TRAPS_*) take the Washington Monument's slot (free again since the Data Center
went), once their art is in work\art (make_traps.py): rules, graphics, name, help and icon.

Writes Data\buildingrules.xml, building_graphics.xml, typenames.xml, help.xml, craftrules.xml,
art\nest_<age>.tga and nest_<age>z.bh3 for every age group made so far (AGES, from work\art, made by
make_nest.py),
art\iface_buildings_wonders.tga (+ _half) with the Nest's icon and art\iface_units_03.tga (+ _half).
"""
import os
import re
import shutil
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
WORK_ART = os.path.join(HERE, "..", "work", "art")
GEMINI = os.path.join(HERE, "..", "gemini", "sandbag_nest_square.jpg")  # the build menu icon
# The Nest's look per age: building_graphics.xml entry suffix -> sprite. An entry holds until the
# next one listed (AGE0 = ages 1-3, AGE3 = 4, AGE4 = 5, AGE5 = 6, AGE6 = 7-8); groups without art
# yet are left out and keep the previous one.
AGES = [("AGE0", "nest_0"), ("AGE3", "nest_3"), ("AGE4", "nest_4"), ("AGE5", "nest_5"), ("AGE6", "nest_6")]
# Sprites the game has no entry for, swapped in at draw time by nation_art.c ([all] in
# nation_art.ini): nest_7 is the Information Age look, drawn instead of nest_6 in age 8.
EXTRA_ART = ["nest_7.tga"]
BASES = {  # newest first: the first folder that has the file is built on
    "buildingrules.xml": [os.path.join(ROOT, "data-center", "mod_override", "Data")],
    "building_graphics.xml": [os.path.join(ROOT, "data-center", "mod_override", "Data")],
    "typenames.xml": [os.path.join(ROOT, "paradrop", "mod_override", "Data"),
                      os.path.join(ROOT, "data-center", "mod_override", "Data")],
    "help.xml": [os.path.join(ROOT, "paradrop", "mod_override", "Data"),
                 os.path.join(ROOT, "data-center", "mod_override", "Data")],
    "craftrules.xml": [os.path.join(ROOT, "paradrop", "mod_override", "Data")],
}
# The buttons' icons go on the unit icon page TEX_UNITS_2 (art\iface_units_03.tga, + _half), in
# cells no unit uses. Not on TEX_ORDERS_0: its cells are also drawn by the game's own buttons (the
# Restock Supplies cell (7, 4) is the cities' rally flag).
UNITS_ATLAS = "iface_units_03.tga"
ICONS = os.path.join(HERE, "..", "icons")

# The button is an unused craft; entrench.c puts it on the panel and carries it out. Without
# b / c / d flags the game casts a craft at once, through its spell command.
# (craft name, GRAPH for help.xml, shown name, help text, icon file, (icon page, cell), FLAGS)
CRAFTS = [
    ("Move Capital Here", "MOVECAPITAL", "Leave Fortification",
     "Every squad dug into this Field Fortification leaves it. The fortification stays and can be filled again.",
     "exit.png", ("TEX_UNITS_2", (7, 10)), ""),
]
CRAFT_FIELDS = {"PREQ0": "None", "COST2": "", "MANA": "0"}  # switched on; free to use (the DLL charges)

SLOT = "Lenin Statue"
NAME = "Field Fortification"  # its look changes with the ages (AGES)
GRAPH = "LENINSTATUE"

RULES = {  # buildingrules.xml fields of the slot
    "OBJ_MASKS": "R",
    "BUILD_FLAGS": "ean",         # land, outside city radius, not captured with the city (like the Tower)
    "PREQ0": "The Art of War",    # like the Tower; entrench.c keeps it out of the Citizens' menu
    "COST": "6t",                 # x10: 60 Timber, rising with each Nest (SUPPORT)
    "SUPPORT0": "timber",
    "SUPPORTVALUE0": "4",
    "SUPPORT1": "metal",
    "SUPPORTVALUE1": "2",
    "PLUNDER_GOOD": "timber",
    "PLUNDER": "10",
    "CAT": "M",
    "JOB_TIME": "300",
    "HITS": "375",                # shown in the help; entrench.c sets 50% of the Tower line's for the age
    "TO_HIT": "0",
    "ATTACK": "0",
    "RANGE": "0-0rng",
    "AMMO_PER_ATT": "0",
    "MOST_SHOTS": "0",
    "BASE_ARROWS": "0",
    "GARRISON_MAX": "3",          # one squad of 3 soldiers; the game's garrison command needs room
    "RECHARGE": "0",
    "ARMOR": "6",
    "X_SIZE": "2",
    "Y_SIZE": "2",
    "LOS": "12",
    "GRID_X": "4",                # military menu, next to the Lookout line
    "GRID_Y": "1",
}

GRAPHICS = (  # building_graphics.xml entry of the slot; offsets from make_nest.py
    '<BUILD name="LENINSTATUE-DEFAULT-{age}" texture=".\\art\\{name}.tga" '
    'zbuffer=".\\art\\{name}z.bh3" anim="" constructsize="0" spritexoff="{x}" spriteyoff="{y}" '
    'halfoffset="0" mask="2x2 solid" doobermask="2x2 doobers">\r\n'
    '      <BOUNDINGVOLUMES>\r\n'
    '        <BOX x="0" y="0" z="12" x_width="84" y_width="84" z_width="24"/>\r\n'
    '      </BOUNDINGVOLUMES>\r\n'
    '    </BUILD>'
)

HELP = ("Low field fortification for one squad of ranged infantry, built by the infantry themselves (the "
        "build button on their panel), who dig into it when it is finished. Right-click it with ranged "
        "infantry (or use the garrison button and its hotkey) to dig in: an earth bank "
        "with a palisade at first, gabions, then sandbags. "
        "Dug in, they hold their ground and fight with their own weapons: +1 range, attacks 40% faster, "
        "+20% damage. The fortification takes every hit meant for them until it is destroyed; it has half "
        "the hit points of a Tower of your age. Melee units cannot dig in. "
        "Digging in costs 35% of the squad's price, +10% for every squad you already have dug in (up to +150%). "
        "The Leave Fortification button empties it.")

# The Tank Traps (2026-11-05, the user: "tank traps ... ocupa o celula si blocheaza trecerea si
# trebuie distrusa ca sa treci ... construita de villager din panoul de military"): a 1x1 building
# in the Washington Monument's slot (free again since the Data Center went, 2026-10-29), built by
# Citizens from the military build menu. A building's footprint blocks every unit; enemies have
# to destroy it to get through (Unit::resolve_block makes a unit attack an enemy building that
# blocks its way, entrenchment/ENGINE_NOTES.md). entrench.c keeps computer players from planning
# it. Its art: tools/make_traps.py (work/art/tank_traps_<i>.tga, four variations; nation_art.ini
# gives every trap one of them).
TRAPS_SLOT = "Washington Monument"
TRAPS_NAME = "Tank Traps"
TRAPS_GRAPH = "WASHINGTONMONUMENT"
TRAPS_ART = "tank_traps"
TRAPS_VARIANTS = 4
TRAPS_RULES = {
    "OBJ_MASKS": "",              # no weapon
    "BUILD_FLAGS": "ean",         # land, outside city radius (in your territory), not captured with the city
    "PREQ0": "Modern Age",       # ages 7 and 8 (the user, 2026-11-05)
    "COST": "2m",                 # x10: 20 Metal, +5 for every one already owned (the user, 2026-11-05: 50 was too dear)
    "SUPPORT0": "metal",
    "SUPPORTVALUE0": "5",
    "SUPPORT1": "none",
    "SUPPORTVALUE1": "0",
    "PLUNDER_GOOD": "metal",
    "PLUNDER": "0",
    "CAT": "M",
    "JOB_TIME": "200",            # 13 seconds for one Citizen
    "HITS": "1200",
    "TO_HIT": "0",
    "ATTENUATE": "0",
    "ATTACK": "0",
    "RANGE": "0-0rng",
    "AMMO_PER_ATT": "0",
    "MOST_SHOTS": "0",
    "BASE_ARROWS": "0",
    "GARRISON_MAX": "0",
    "RECHARGE": "0",
    "ARMOR": "8",                 # the Redoubt's: rifles barely scratch them
    "X_SIZE": "1",
    "Y_SIZE": "1",
    "LOS": "2",
    "GRID_X": "4",                # military menu, right of the Lookout line (the Nest's cell, which
    "GRID_Y": "1",                # Citizens never see: entrench.c leaves the Nest out of their menu)
    "TEX_ID": "TEX_BUILDINGS_3",
    "TEX_COL": "2",               # the Washington Monument's own icon cell
    "TEX_ROW": "5",
}
TRAPS_GRAPHICS = (  # origin = the tile's centre (halfoffset 0), offsets from make_traps.py
    '<BUILD name="{graph}-DEFAULT-AGE0" texture=".\\art\\{name}_0.tga" zbuffer=".\\art\\{name}_0z.bh3" anim="" '
    'constructsize="0" spritexoff="{x}" spriteyoff="{y}" halfoffset="0" mask="1x1 solid" doobermask="1x1 doobers">\r\n'
    '      <BOUNDINGVOLUMES>\r\n'
    '        <BOX x="0" y="0" z="12" x_width="58" y_width="58" z_width="24"/>\r\n'
    '      </BOUNDINGVOLUMES>\r\n'
    '    </BUILD>'
)
TRAPS_HELP = ("Steel anti-tank obstacles on one tile, built by Citizens in the Modern and Information Ages. Nothing can "
              "pass through them, your own units neither: an enemy has to destroy them to get through. Lay "
              "them in a line to close a gap; leave a way open for your own army. 1200 hit points and heavy armor: rifles barely scratch them, tanks, artillery and aircraft break them.")
TRAPS_ICON = os.path.join(HERE, "..", "work", "tank_traps_icon.png")


def traps_ready():
    return os.path.exists(os.path.join(WORK_ART, f"{TRAPS_ART}_0.tga"))


ICON_CELL = (1, 5)                 # the Lenin Statue's cell on TEX_BUILDINGS_3
CELL_STEP, CELL_SIZE = (62, 40), (61, 39)


def read(path):
    return open(path, "rb").read().decode("utf-8")


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, "wb").write(text.encode("utf-8"))
    print(path)


def base(game, name):
    for d in BASES.get(name, []):
        if os.path.exists(os.path.join(d, name)):
            return os.path.join(d, name)
    return os.path.join(game, "Data", name)


def set_fields(block, fields, what):
    for tag, value in fields.items():
        block, n = re.subn(rf"<{tag}>[^<]*</{tag}>|<{tag}/>", f"<{tag}>{value}</{tag}>" if value else f"<{tag}/>",
                           block)
        if n != 1:
            sys.exit(f"{what}: {tag} found {n} times")
    return block


def building_rules(text):
    start = text.index(f"<NAME>{SLOT}</NAME>")
    end = text.index("</BUILDING>", start)
    text = text[:start] + set_fields(text[start:end], RULES, SLOT) + text[end:]
    if traps_ready():
        start = text.index(f"<NAME>{TRAPS_SLOT}</NAME>")
        end = text.index("</BUILDING>", start)
        text = text[:start] + set_fields(text[start:end], TRAPS_RULES, TRAPS_SLOT) + text[end:]
    return text


def ages():
    for age, name in AGES:
        if os.path.exists(os.path.join(WORK_ART, f"{name}.tga")):
            x, y = map(int, open(os.path.join(WORK_ART, "..", f"offsets_{name}.txt")).read().split())
            yield age, name, x, y


def building_graphics(text):
    entries = "\r\n    ".join(GRAPHICS.format(age=age, name=name, x=x, y=y) for age, name, x, y in ages())
    new, n = re.subn(rf'<BUILD name="{GRAPH}-DEFAULT-AGE0".*?</BUILD>', lambda m: entries, text, flags=re.S)
    if n != 1:
        sys.exit(f"building_graphics.xml: slot found {n} times")
    if traps_ready():
        x, y = map(int, open(os.path.join(WORK_ART, "..", f"offsets_{TRAPS_ART}.txt")).read().split())
        entry = TRAPS_GRAPHICS.format(graph=TRAPS_GRAPH, name=TRAPS_ART, x=x, y=y)
        new, n = re.subn(rf'<BUILD name="{TRAPS_GRAPH}-DEFAULT-AGE0".*?</BUILD>', lambda m: entry, new, flags=re.S)
        if n != 1:
            sys.exit(f"building_graphics.xml: {TRAPS_GRAPH} found {n} times")
    return new


def type_names(text):
    text, n = re.subn(rf'(<STRING hash="\d+" needed="1">){re.escape(SLOT)}(</STRING>)', rf"\g<1>{NAME}\g<2>", text)
    if n != 1:
        sys.exit(f"typenames.xml: {SLOT} found {n} times")
    if traps_ready():
        text, n = re.subn(rf'(<STRING hash="\d+" needed="1">){re.escape(TRAPS_SLOT)}(</STRING>)',
                          rf"\g<1>{TRAPS_NAME}\g<2>", text)
        if n != 1:
            sys.exit(f"typenames.xml: {TRAPS_SLOT} found {n} times")
    return text


def help_entry(text, graph, desc):
    entry = f'<ENTRY name="{graph}">\r\n\t\t\t<STRING>\r\n\t\t\t\t<BULLET/>{desc}</STRING>\r\n\t\t</ENTRY>'
    text, n = re.subn(rf'<ENTRY name="{graph}">.*?</ENTRY>', lambda m: entry, text, flags=re.S)
    if n == 0:  # the statues had no help: add it before the Tower's
        i = text.index('<ENTRY name="TOWER">')
        text = text[:i] + entry + "\r\n\t\t" + text[i:]
    return text


def help_text(text):
    text = help_entry(text, GRAPH, HELP)
    if traps_ready():
        text = help_entry(text, TRAPS_GRAPH, TRAPS_HELP)
    return text


def craft_rules(text):
    for name, *_, (page, (col, row)), flags in CRAFTS:
        m = re.search(rf"<CRAFT>\s*<NAME>{re.escape(name)}</NAME>.*?</CRAFT>", text, re.S)
        if not m:
            sys.exit(f"craftrules.xml: {name} not found")
        fields = dict(CRAFT_FIELDS, FLAGS=flags, TEX_ID=page, TEX_COL=str(col), TEX_ROW=str(row))
        text = text[:m.start()] + set_fields(m.group(0), fields, name) + text[m.end():]
    return text


def craft_names(text):
    for name, _, shown, *_ in CRAFTS:
        text, n = re.subn(rf'(<STRING hash="\d+" needed="1">){re.escape(name)}(</STRING>)', rf"\g<1>{shown}\g<2>",
                          text)
        if n != 1:
            sys.exit(f"typenames.xml: {name} found {n} times")
    return text


def craft_help(text):
    for _, graph, _, desc, *_ in CRAFTS:
        entry = f'<ENTRY name="{graph}">\r\n\t\t\t<STRING>\r\n\t\t\t\t<BULLET/>{desc}</STRING>\r\n\t\t</ENTRY>'
        text, n = re.subn(rf'<ENTRY name="{graph}">.*?</ENTRY>', lambda m: entry, text, flags=re.S)
        if n == 0:  # a craft's help goes with the other crafts' (RALLY, PARADROP...), at the end of <SPELLS>
            i = text.index("\t</SPELLS>")
            text = text[:i] + "\t\t" + entry + "\r\n" + text[i:]
    return text


def button_icons(game, out):
    for name, step, size in ((UNITS_ATLAS, CELL_STEP, CELL_SIZE), (UNITS_ATLAS.replace(".tga", "_half.tga"), (31, 20), (30, 19))):
        atlas = Image.open(os.path.join(game, "art", name)).convert("RGBA")
        for *_, file, (page, (col, row)), _ in CRAFTS:
            if not file:
                continue
            icon = Image.open(os.path.join(ICONS, file)).convert("RGBA").resize(size, Image.LANCZOS)
            atlas.paste(icon, (col * step[0], row * step[1]))
        path = os.path.join(out, "art", name)
        atlas.save(path)
        print(path)


def icon():
    """The ring from the Gemini image on a grass background, 61 x 39."""
    sys.path.insert(0, HERE)
    import make_nest
    img = Image.open(GEMINI).convert("RGB")
    a = make_nest.keyed(img)
    box = a.point(lambda v: 255 if v >= 128 else 0).getbbox()
    obj = img.crop(box)
    am = a.crop(box)
    w, h = CELL_SIZE
    s = min((w - 2) / obj.width, (h - 2) / obj.height) * 1.15  # a little larger, cropped at the sides
    ow, oh = round(obj.width * s), round(obj.height * s)
    yy = np.linspace(0, 1, h)[:, None, None]
    grass = (np.array([86, 118, 52]) * (1 - yy) + np.array([60, 92, 38]) * yy) * np.ones((h, w, 1))
    cell = Image.fromarray(grass.astype(np.uint8))
    cell.paste(obj.resize((ow, oh), Image.LANCZOS), ((w - ow) // 2, h - oh + 2), am.resize((ow, oh), Image.LANCZOS))
    return cell


def icons(game, out):
    cell = icon()
    for name, step, size in (("iface_buildings_wonders.tga", CELL_STEP, CELL_SIZE),
                             ("iface_buildings_wonders_half.tga", (31, 20), (30, 19))):
        dc = os.path.join(ROOT, "data-center", "mod_override", "art", name)  # holds the Data Center Expansions' icons
        atlas = Image.open(dc if os.path.exists(dc) else os.path.join(game, "art", name)).convert("RGBA")
        x, y = ICON_CELL[0] * step[0], ICON_CELL[1] * step[1]
        atlas.paste(cell.resize(size, Image.LANCZOS).convert("RGBA"), (x, y))
        if traps_ready() and os.path.exists(TRAPS_ICON):
            col, row = int(TRAPS_RULES["TEX_COL"]), int(TRAPS_RULES["TEX_ROW"])
            traps = Image.open(TRAPS_ICON).convert("RGBA").resize(size, Image.LANCZOS)
            atlas.paste(traps, (col * step[0], row * step[1]))
        path = os.path.join(out, "art", name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        atlas.save(path)
        print(path)


def main(game, out):
    out_data = os.path.join(out, "Data")
    write(os.path.join(out_data, "buildingrules.xml"), building_rules(read(base(game, "buildingrules.xml"))))
    write(os.path.join(out_data, "building_graphics.xml"),
          building_graphics(read(base(game, "building_graphics.xml"))))
    write(os.path.join(out_data, "typenames.xml"), craft_names(type_names(read(base(game, "typenames.xml")))))
    write(os.path.join(out_data, "help.xml"), craft_help(help_text(read(base(game, "help.xml")))))
    write(os.path.join(out_data, "craftrules.xml"), craft_rules(read(base(game, "craftrules.xml"))))
    os.makedirs(os.path.join(out, "art"), exist_ok=True)
    for f in ("sandbag_nest.tga", "sandbag_nestz.bh3"):  # the first build's names
        if os.path.exists(os.path.join(out, "art", f)):
            os.remove(os.path.join(out, "art", f))
    for _, name, _, _ in ages():
        for f in (f"{name}.tga", f"{name}z.bh3"):
            shutil.copyfile(os.path.join(WORK_ART, f), os.path.join(out, "art", f))
            print(os.path.join(out, "art", f))
    for f in EXTRA_ART:
        if os.path.exists(os.path.join(WORK_ART, f)):
            shutil.copyfile(os.path.join(WORK_ART, f), os.path.join(out, "art", f))
            print(os.path.join(out, "art", f))
    if traps_ready():
        for f in [f"{TRAPS_ART}_{i}.tga" for i in range(TRAPS_VARIANTS)] + [f"{TRAPS_ART}_0z.bh3"]:
            shutil.copyfile(os.path.join(WORK_ART, f), os.path.join(out, "art", f))
            print(os.path.join(out, "art", f))
    else:
        print("tank traps: no art yet (work/art/tank_traps_0.tga, tools/make_traps.py): the slot keeps the game's rules")
    icons(game, out)
    button_icons(game, out)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
