r"""Builds the city districts' data files into mod_override\ from the game's own files.

The game refuses a buildingrules.xml with more buildings than it was compiled with, so:
  - the three districts (2x2) take the slots of three scenario-only buildings: El Dorado, the
    Courtyard Fountain and the last scenario House (HOUSE35). The mod's winmm.dll
    (camera-smooth\src\datacenter.c) gives each player the bonus of the districts they own; they
    are ordered from a city's panel and built by a builder who comes out of the nearest city;
  - the builder takes the slot of a campaign-only unit (TECH_UNIT), made a copy of the Citizen.
The Data Center the districts started as (in the Washington Monument's slot) is gone since
2026-10-29: that slot keeps the game's own rules, and remove_old takes its picture away.

    python build_data.py <game folder> <out mod_override folder>

Writes Data\unitrules.xml, buildingrules.xml, building_graphics.xml, typenames.xml, help.xml,
masks.txt (the game's, with a 1x1 doober mask added), art\district_<kind>.tga and z.bh3 (from
work\art) and art\iface_buildings_wonders.tga (+ _half) with the districts' icons
(icons\district_<kind>.png).
"""
import os
import re
import shutil
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ICONS = os.path.join(HERE, "..", "icons")
WORK_ART = os.path.join(HERE, "..", "work", "art")

SLOT = "Washington Monument"
NAME = "Data Center"

RULES = {  # buildingrules.xml fields of the slot
    "OBJ_MASKS": "",
    "BUILD_FLAGS": "aj",          # land, max 1 per city
    "PREQ0": "Information Age",   # and one of the four last Library techs: datacenter.c
    "COST": "40k/40g",            # x10 in game: 400 Knowledge, 400 Wealth
    "SUPPORT0": "knowledge",
    "SUPPORTVALUE0": "50",
    "SUPPORT1": "wealth",
    "SUPPORTVALUE1": "50",
    "PLUNDER_GOOD": "knowledge",
    "PLUNDER": "50",
    "CAT": "E",
    "JOB_TIME": "900",
    "HITS": "2500",
    "TO_HIT": "0",
    "ATTENUATE": "0",
    "RANGE": "0-0rng",
    "AMMO_PER_ATT": "0",
    "MOST_SHOTS": "0",
    "BASE_ARROWS": "0",
    "GARRISON_MAX": "0",
    "RECHARGE": "0",
    "ARMOR": "0",
    "X_SIZE": "7",
    "Y_SIZE": "7",
    "LOS": "8",
    "GRID_X": "4",                # economy menu, below the University
    "GRID_Y": "1",
}

GRAPHICS = (  # building_graphics.xml entry of the slot, sized like Versailles
    '<BUILD name="WASHINGTONMONUMENT-DEFAULT-AGE0" texture=".\\art\\data_center.tga" '
    'zbuffer=".\\art\\versailles-zbuf.BH3" anim="" constructsize="6" spritexoff="317" spriteyoff="313" '
    'halfoffset="1" mask="7x7 extra space" doobermask="7x7 doobers">\r\n'
    '      <BOUNDINGVOLUMES>\r\n'
    '        <BOX x="-34" y="-17" z="122" x_width="358" y_width="358" z_width="244"/>\r\n'
    '      </BOUNDINGVOLUMES>\r\n'
    '    </BUILD>'
)

HELP = ("End-game building of the Information Age. Needs at least one of the four last Library technologies: "
        "Missile Shield, World Government, Global Prosperity or Artificial Intelligence. You cannot start a "
        "Data Center while you own one. A finished Data Center lets you build Data Center Expansions.")

# The Expansions: (kind, GRAPH of the slot, new internal name or None to keep the slot's, typenames
# string whose text becomes the shown name, shown name, icon cell on TEX_BUILDINGS_3, build menu cell,
# COST (x10): the price of the branch's last one-time upgrade, the same for every one built (no ramp),
# effect). HOUSE35 shares its name
# "House" with 35 other Houses, so it takes "Lighthouse", a spare typenames string no building uses.
# The order and the effects match datacenter.c (DC_EXP_* in ron.h) and data_center.ini.
# 2026-10-29 the user turned the Expansions into city districts (TODO.md "Data Center"): 2x2, built
# from any city's panel next to that city or its districts, 3x the price and 3x the bonus, the price
# rising 6% with each one owned; the Data Center itself is no longer offered (datacenter.c).
# 2026-10-29 again (the user): Housing +10 for 6500 Food + 3400 Knowledge, Commerce +30 for 3000
# Knowledge + 3000 Wealth, Industrial +3% for 3000 Knowledge + 2000 Wealth, rising 5% each.
EXPANSIONS = [
    ("housing", "ELDORADO", None, "El Dorado", "Housing District", (0, 5), (4, 2),
     "650f/340k", "+10 population limit, beyond the usual maximum of 200"),
    ("commerce", "COURTYARDFOUNTAIN", None, "Courtyard Fountain", "Commerce District", (3, 5), (4, 3),
     "300k/300g", "+30 to the resource-rate limit of every resource, beyond the usual 999"),
    ("industry", "HOUSE35", "Lighthouse", "Lighthouse", "Industrial District", (3, 4), (3, 3),
     "300k/200g", "+3% to all resource income"),
]

EXP_RULES = {  # the same for every Expansion
    "OBJ_MASKS": "",
    "BUILD_FLAGS": "ae",          # land, also outside a city's radius (in your territory)
    "PREQ0": "none",              # unlocked by datacenter.c: every Library technology up to the game's last age
    "PREQ1": "none",
    "PREQ2": "none",
    "FROM": "none",
    "PLUNDER_GOOD": "knowledge",
    "PLUNDER": "20",
    "CAT": "E",
    "JOB_TIME": "600",
    "HITS": "1500",
    "TO_HIT": "0",
    "ATTENUATE": "0",
    "ATTACK": "0",
    "RANGE": "0-0rng",
    "GARRISON_MAX": "0",
    "ARMOR": "0",
    "X_SIZE": "2",
    "Y_SIZE": "2",
    "LOS": "4",
    "TEX_ID": "TEX_BUILDINGS_3",
}

EXP_GRAPHICS = (  # offsets from district_art.py (work\offsets_<name>.txt): the plot's centre;
    # halfoffset 0 like the game's own 2x2 buildings (mines, woodcutters)
    '<BUILD name="{graph}-DEFAULT-AGE0" texture=".\\art\\{name}.tga" zbuffer=".\\art\\{name}z.bh3" anim="" '
    'constructsize="0" spritexoff="{x}" spriteyoff="{y}" halfoffset="0" mask="2x2 solid" doobermask="2x2 doobers">\r\n'
    '      <BOUNDINGVOLUMES>\r\n'
    '        <BOX x="0" y="0" z="55" x_width="106" y_width="106" z_width="110"/>\r\n'
    '      </BOUNDINGVOLUMES>\r\n'
    '    </BUILD>'
)

# The technician who builds the Expansions (datacenter.c makes and commands him): a campaign-only
# unit's slot made a copy of the Citizen, so he walks, builds and is drawn like one. Not trained
# anywhere (WHERE None), every nation.
TECH_UNIT = 341                    # unitrules.xml index (was the campaign's Barbarian Archers)
TECH_NAME = "District Builder"
TECH_FIELDS = {"WHERE": "None", "TRIBE_MASK": "1" * 24, "HITS": "80", "COST": "0f", "SUPPORT": "support",
               "GRID_X": "0", "GRID_Y": "0"}

# masks.txt has no doober mask for one tile
MASK_1X1 = "\r\n#1x1 doobers\r\n1, 1\r\nup\r\n"

ICON_PAGE = "iface_buildings_wonders.tga"  # TEX_BUILDINGS_3, 4 x 6 cells
CELL_STEP, CELL_SIZE = (62, 40), (61, 39)


def read(path):
    return open(path, "rb").read().decode("utf-8")


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, "wb").write(text.encode("utf-8"))
    print(path)


def sprite_name(kind):
    return f"district_{kind}"


def set_fields(block, fields, what):
    for tag, value in fields.items():
        block, n = re.subn(rf"<{tag}>[^<]*</{tag}>|<{tag}/>", f"<{tag}>{value}</{tag}>" if value else f"<{tag}/>",
                           block)
        if n != 1:
            sys.exit(f"{what}: {tag} found {n} times")
    return block


def building_block(text, tag, value):
    """Start and end of the <BUILDING> whose <tag> is value."""
    at = text.index(f"<{tag}>{value}</{tag}>")
    return text.rindex("<BUILDING>", 0, at), text.index("</BUILDING>", at)


RAMP = 0.05  # every district of a kind already owned makes the next one of that kind 5 % dearer
RESOURCES = {"f": "food", "t": "timber", "g": "wealth", "m": "metal", "k": "knowledge", "o": "oil"}


def ramp(cost):
    """SUPPORT0/1 and SUPPORTVALUE0/1 for a COST such as "500f/250k": the game adds SUPPORTVALUE
    (in resource units; COST is in tens) for every building of the type the player owns."""
    fields = {}
    parts = cost.split("/")
    for i in range(2):
        if i < len(parts):
            amount, res = int(parts[i][:-1]), RESOURCES[parts[i][-1]]
            fields[f"SUPPORT{i}"], fields[f"SUPPORTVALUE{i}"] = res, str(round(amount * 10 * RAMP))
        else:
            fields[f"SUPPORT{i}"], fields[f"SUPPORTVALUE{i}"] = "none", "0"
    return fields


def building_rules(text):
    # 2026-10-29 (the user: "data center trebuie sa dispara"): the Washington Monument slot keeps
    # the game's own rules (PREQ0 disable: never offered), name, graphics and help; RULES,
    # GRAPHICS and HELP above are what the Data Center was, kept for the record.
    for kind, graph, new_name, _, shown, (col, row), (gx, gy), cost, _ in EXPANSIONS:
        fields = dict(EXP_RULES, COST=cost, TEX_COL=str(col), TEX_ROW=str(row), GRID_X=str(gx), GRID_Y=str(gy),
                      **ramp(cost))
        if new_name:
            fields["NAME"] = fields["TYPENAME"] = new_name
        start, end = building_block(text, "GRAPH", graph)
        text = text[:start] + set_fields(text[start:end], fields, shown) + text[end:]
    return text


def offsets(name):
    path = os.path.join(WORK_ART, "..", f"offsets_{name}.txt")
    if not os.path.exists(path):
        sys.exit(f"{path} missing: run make_expansion.py placeholder first")
    return map(int, open(path).read().split())


def building_graphics(text):
    entries = []
    for kind, graph, *_ in EXPANSIONS:
        x, y = offsets(sprite_name(kind))
        entries.append((graph, EXP_GRAPHICS.format(graph=graph, name=sprite_name(kind), x=x, y=y)))
    for graph, entry in entries:
        text, n = re.subn(rf'<BUILD name="{graph}-DEFAULT-AGE0".*?</BUILD>', lambda m: entry, text, flags=re.S)
        if n != 1:
            sys.exit(f"building_graphics.xml: {graph} found {n} times")
    return text


def unit_blocks(text):
    return [m.span() for m in re.finditer(r"<UNIT>.*?</UNIT>", text, re.S)]


def unit_field(block, tag):
    return re.search(rf"<{tag}>([^<]*)</{tag}>", block).group(1)


def unit_rules(text):
    """The technician: the Citizen's rules under the slot's own name."""
    spans = unit_blocks(text)
    if len(spans) != 364:
        sys.exit(f"unitrules.xml: {len(spans)} units, expected 364")
    s, e = spans[TECH_UNIT]
    slot = text[s:e]
    if unit_field(slot, "WHERE") != "None":
        sys.exit(f"unitrules.xml: unit {TECH_UNIT} ({unit_field(slot, 'NAME')}) is not a campaign-only unit")
    citizen = text[spans[0][0]:spans[0][1]]
    name = unit_field(slot, "NAME")
    block = set_fields(citizen, dict(TECH_FIELDS, NAME=name, TYPENAME=name), "technician")
    return text[:s] + block + text[e:]


def type_names(text, units_text):
    # the hash is the game's hash of the internal name, so it stays; only the shown text changes
    tech = unit_field(units_text[slice(*unit_blocks(units_text)[TECH_UNIT])], "NAME")
    names = [(tech, TECH_NAME)] + [(key, shown) for _, _, _, key, shown, *_ in EXPANSIONS]
    for old, new in names:
        text, n = re.subn(rf'(<STRING hash="\d+" needed="1">){re.escape(old)}(</STRING>)', rf"\g<1>{new}\g<2>", text)
        if n != 1:
            sys.exit(f"typenames.xml: {old} found {n} times")
    return text


def help_text(text):
    entries = []
    for _, graph, *_, effect in EXPANSIONS:
        entries.append((graph, f"City district: {effect}, for whoever holds it. Built from the panel of any of your "
                               f"cities, on a 2x2 plot next to that city or to one of its districts (at most 2 tiles "
                               f"between them, so units can pass), by a builder who comes out of the nearest city. "
                               f"Unlocked once you have researched every Library technology up to the last Age of "
                               f"the game (the four last ones excepted). Build as many as you like: their bonuses "
                               f"add up, and each one of this kind you own makes the next {RAMP:.0%} dearer. The "
                               f"bonus is lost when the district is destroyed."))
    for graph, desc in entries:
        entry = f'<ENTRY name="{graph}">\r\n\t\t\t<STRING>\r\n\t\t\t\t<BULLET/>{desc}</STRING>\r\n\t\t</ENTRY>'
        text, n = re.subn(rf'<ENTRY name="{graph}">.*?</ENTRY>', lambda m: entry, text, flags=re.S)
        if n == 0:  # the scenario buildings had no help: add it before the Tower's
            i = text.index('<ENTRY name="TOWER">')
            text = text[:i] + entry + "\r\n\t\t" + text[i:]
    return text


def masks(game, out):
    text = open(os.path.join(game, "masks.txt"), "rb").read().decode("latin-1")
    if "#1x1 doobers" not in text:
        text = text.rstrip() + "\r\n" + MASK_1X1
    path = os.path.join(out, "masks.txt")
    open(path, "wb").write(text.encode("latin-1"))
    print(path)


def art(out):
    os.makedirs(os.path.join(out, "art"), exist_ok=True)
    for kind, *_ in EXPANSIONS:
        name = sprite_name(kind)
        for f in (f"{name}.tga", f"{name}z.bh3"):
            shutil.copyfile(os.path.join(WORK_ART, f), os.path.join(out, "art", f))
            print(os.path.join(out, "art", f))


def icons(game, out):
    for name, step, size in ((ICON_PAGE, CELL_STEP, CELL_SIZE),
                             (ICON_PAGE.replace(".tga", "_half.tga"), (31, 20), (30, 19))):
        atlas = Image.open(os.path.join(game, "art", name)).convert("RGBA")
        for kind, _, _, _, _, (col, row), *_ in EXPANSIONS:
            icon = Image.open(os.path.join(ICONS, f"district_{kind}.png")).convert("RGBA").resize(size, Image.LANCZOS)
            atlas.paste(icon, (col * step[0], row * step[1]))
        path = os.path.join(out, "art", name)
        atlas.save(path)
        print(path)


def remove_old(out):
    """Files of earlier versions, no longer made: the first version's upgrade icons, the Data
    Center's picture and the 1x1 Expansions' art (dc_exp_*, replaced by the districts)."""
    art_dir = os.path.join(out, "art")
    old = ["iface_units_03.tga", "iface_units_03_half.tga", "data_center.tga"]
    old += [f for f in os.listdir(art_dir) if f.startswith("dc_exp_")] if os.path.isdir(art_dir) else []
    for f in old:
        if os.path.exists(os.path.join(art_dir, f)):
            os.remove(os.path.join(art_dir, f))
            print("removed", os.path.join(art_dir, f))


def main(game, out):
    data, out_data = os.path.join(game, "Data"), os.path.join(out, "Data")
    remove_old(out)
    units = read(os.path.join(data, "unitrules.xml"))
    write(os.path.join(out_data, "unitrules.xml"), unit_rules(units))
    write(os.path.join(out_data, "buildingrules.xml"), building_rules(read(os.path.join(data, "buildingrules.xml"))))
    write(os.path.join(out_data, "building_graphics.xml"),
          building_graphics(read(os.path.join(data, "building_graphics.xml"))))
    write(os.path.join(out_data, "typenames.xml"), type_names(read(os.path.join(data, "typenames.xml")), units))
    write(os.path.join(out_data, "help.xml"), help_text(read(os.path.join(data, "help.xml"))))
    masks(game, out)
    art(out)
    icons(game, out)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(*sys.argv[1:3])
