r"""Builds the Romania mod's data files into mod_override\.

    python build_data.py <game folder> <out mod_override folder>

Romania is a 25th nation, tribe 24 (the exe side is camera-smooth\src\romania.c). The data:

- Data\rules.xml: the game's, with Romania's <TRIBE> after the Persians in <TRIBES> (the DLL
  turns itself on only when the rules.xml the game reads lists 25 nations).
- tribes\romania.xml: name, leaders, cities, unit and building styles (source\romania.xml).
- Data\unitrules.xml, buildingrules.xml, techrules.xml, craftrules.xml: every TRIBE_MASK gets a
  25th bit, Romania's (the leftmost character; String::binary reads them high bit first).
  Buildings, techs and crafts: Romania has what the Russians have (all of them have either every
  nation or none). Units: in each line (a unit and the units whose <GRAFT> names it) Romania
  gets the member the Russians have when it is the same unit in other art (same <NAME>), and
  the most common member with the line's own name where the Russians have a unique unit
  (Cossacks, Red Guards, T-80, Katyusha...). Lines the Russians do not have, Romania does not
  have either.
- Data\help.xml: TRIBE24, the nation power shown in the setup screen and the game, with the
  numbers read from rules.xml.

Every file is built on the newest version another mod of this repo made of it (BASES), so
build those mods first; after rebuilding one of them, build this one again, or Romania has no
units at all (24-bit masks).
"""
import os
import re
import shutil
import struct
import sys

import numpy as np
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
SOURCE = os.path.join(HERE, "..", "source")
ICONS = os.path.join(HERE, "..", "icons")  # <unit name in lower case, _ for the rest>.png
ATLAS_COLS, ATLAS_ROWS = 8, 12  # the game's icon pages: 61 x 39 cells, 62 x 40 apart (_half: 30 x 19, 31 x 20)
ROMANIA = 24
TEMPLATE = 13  # the Russians
# mods whose mod_override\Data files are built on each other, newest first
MOD_ORDER = ["citizen-icons", "nuke-fx", "general-heal", "entrenchment", "paradrop", "data-center"]
RULE_FILES = {"unitrules.xml": "UNIT", "buildingrules.xml": "BUILDING", "techrules.xml": "TECH",
              "craftrules.xml": "CRAFT"}

TRIBE_ENTRY = """    <TRIBE>
      <FILE>ROMANIA.XML  </FILE>
      <KEY>romania  </KEY>
    </TRIBE>
"""

# The nation power: pieces of other nations' powers (romania.c k_bonus, romania.ini [power]).
# Each line: (rules.xml constants read as numbers, text with {0}, {1}... for them)
POWER_TITLE = "{Romania has the Power of the Carpathians.}"
POWER = [
    (["RUSSIAN_ATTRITION"], "Attrition damage to enemy units in your territory increased by {0}%. Attrition upgrades are free."),
    (["RUSSIAN_OIL"], "+{0}% Oil gathering."),
    (["ROMAN_FORT_COST", "ROMAN_FORT_SPEED", "ROMAN_FORT_BORDERS"],
     "Forts and Towers {0}% cheaper. Forts {1}% faster to build, exert +{2} National Borders, and get free upgrades."),
    ([], "Receive the Militia, Minuteman, and Partisan upgrades for free."),
]


def read(path):
    with open(path, encoding="utf-8", newline="") as fh:  # line endings kept as they are
        return fh.read()


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as fh:
        fh.write(text)


def newline(text):
    return "\r\n" if "\r\n" in text else "\n"


def base_of(game, name):
    for mod in MOD_ORDER:
        p = os.path.join(ROOT, mod, "mod_override", "Data", name)
        if os.path.exists(p):
            return p
    return os.path.join(game, "Data", name)


def constant(rules, name):
    m = re.search(r'<%s value="(-?\d+)' % name, rules)
    if not m:
        raise SystemExit("rules.xml: no number in " + name)
    return int(m.group(1))


def build_rules(game):
    text = read(base_of(game, "rules.xml"))  # the bug fixes change some constants
    start = text.index("<TRIBES>")
    end = text.index("</TRIBES>", start)
    if "ROMANIA.XML" in text[start:end]:
        return text
    n = text[start:end].count("<TRIBE>")
    if n != 24:
        raise SystemExit("rules.xml lists %d nations, 24 expected" % n)
    line_start = text.rindex("\n", 0, end) + 1
    return text[:line_start] + TRIBE_ENTRY + text[line_start:]


def mask24(mask):
    if len(mask) == 25:  # built on a file that has Romania already
        mask = mask[1:]
    if len(mask) != 24:
        raise SystemExit("TRIBE_MASK of %d characters: %s" % (len(mask), mask))
    return int(mask, 2)


def romania_bits(text, tag):
    """Romania's bit for every type of the file, in order."""
    blocks = re.findall(r"<%s>(.*?)</%s>" % (tag, tag), text, re.S)
    types = []
    for b in blocks:
        name = re.search(r"<NAME>(.*?)</NAME>", b).group(1).strip()
        g = re.search(r"<GRAFT>(.*?)</GRAFT>", b)
        graft = g.group(1).strip() if g else "none"
        m = re.search(r"<TRIBE_MASK>([01]+)</TRIBE_MASK>", b)
        types.append(dict(name=name, graft=graft, bits=mask24(m.group(1)) if m else None))
    if tag != "UNIT":
        return [t["bits"] is not None and t["bits"] >> TEMPLATE & 1 for t in types], []
    first = {}
    for i, t in enumerate(types):
        first.setdefault(t["name"].lower(), i)
    lines = {}
    for i, t in enumerate(types):
        root = i if t["graft"].lower() == "none" else first.get(t["graft"].lower(), i)
        lines.setdefault(root, []).append(i)
    bits = [0] * len(types)
    report = []
    for root, members in lines.items():
        base = types[root]["name"]
        have = [i for i in members if (types[i]["bits"] or 0) >> TEMPLATE & 1]
        if not have:
            continue
        if types[have[0]]["name"] == base:
            pick = have[0]
        else:
            same = [i for i in members if types[i]["name"] == base and types[i]["bits"]]
            pick = max(same, key=lambda i: bin(types[i]["bits"]).count("1"))
            report.append("%s: the Russians' %s, Romania the %s of %d nations" % (
                base, types[have[0]]["name"], base, bin(types[pick]["bits"]).count("1")))
        bits[pick] = 1
    return bits, report


def extend_masks(text, tag):
    bits, report = romania_bits(text, tag)
    blocks = list(re.finditer(r"<%s>(.*?)</%s>" % (tag, tag), text, re.S))
    out, pos = [], 0
    for b, bit in zip(blocks, bits):
        body = b.group(0)
        body = re.sub(r"<TRIBE_MASK>([01]+)</TRIBE_MASK>",
                      lambda m: "<TRIBE_MASK>%d%s</TRIBE_MASK>" % (bit, format(mask24(m.group(1)), "024b")), body)
        out.append(text[pos:b.start()])
        out.append(body)
        pos = b.end()
    out.append(text[pos:])
    return "".join(out), sum(bits), report


# Romania's own units: quality against quantity, the price following the power (the user,
# 2026-10-26; the population cost is left out of this balance, to be looked at another time).
# Each takes a campaign-only slot (WHERE None, no nation; the ones campaigns use least) and becomes
# a copy of the unit Romania had in that tier, with GRAFT naming the generic unit, FLAGS + y (a
# unique unit) and only Romania's bit. Its cost is scaled by the target and rounded (the game keeps
# costs in tens), then attack and hit points are scaled by the ratio the cost really got
# (power = sqrt(attack / recharge x hits)), so power per resource stays the generic unit's.
# Training time unchanged. (slot index in unitrules.xml, its internal name, generic unit, shown
# name, target change of power and cost)
ROMANIAN_UNITS = [
    # elite, dearer
    (328, "Junkers", "Phalanx", "Falxmen", 1.20),
    (336, "Red Coats", "Cataphract", "Roxolani Lancers", 1.15),
    (329, "Patrol Jeep", "Heavy Knight", "Viteji", 1.15),
    (340, "Humvee", "Infantry", "Vanatori de Munte", 1.15),
    # levies, reservists and outdated machines, cheaper
    (335, "Elite Bandeirantes", "Javelineers", "Comati", 0.90),
    (346, "Galleon", "Pikemen", "Voinici", 0.85),
    (347, "Ironclad", "Arquebusiers", "Darabani", 0.90),
    (345, "Imperial Guard", "Riflemen", "Dorobanti", 0.90),
    (339, "Patrol Boat", "Armored Car", "Automitraliera", 0.85),
    # the R-2 is Romania's Tank of the Modern Age (the user, 2026-10-29: "tancul R-2 e in epoca 7");
    # the Industrial Age's Light Tank before it is the Renault FT the Romanian army had from 1919
    (349, "Catapult Ship", "Light Tank", "Renault FT", 0.80),
    (348, "Nuclear Missile Sub", "Tank", "R-2", 0.80),
    # a model of its own, the generic's power and cost
    (342, "Barbarian Javelineers", "Fighter", "IAR-80", 1.0),
]

# Own units with art of their own: slot -> (the slot's graphics family, taken over; the family
# whose sounds and help text it gets; model; texture). The family's unit_graphics.xml entries
# become one entry copied from the template entry (the generic's look, its animations and
# events) with the new model and texture (tools/make_plane.py, tools/build_plane_bh3.py).
OWN_ART = {
    342: ("BARBARIANJAVELINEERS", "FIGHTER", "P40-DEFAULT-AGE0", r".\art\iar80.bh3", r".\art\iar80.tga"),
    # R-2, the Modern Age Tank: the slot's own family (the campaign's Nuclear Missile Sub), the
    # Panzer's entry and skeleton, the medium tank's sounds and help
    348: ("TYPHOON", "MEDIUMTANK", "PANZER-DEFAULT-AGE0", r".\art\r2.bh3", r".\art\r2.tga"),
}


def field(block, tag):
    m = re.search(r"<%s>(.*?)</%s>" % (tag, tag), block)
    return m.group(1) if m else None


def set_field(block, tag, value):
    new, n = re.subn(r"<%s>.*?</%s>" % (tag, tag), lambda m: "<%s>%s</%s>" % (tag, value, tag), block, count=1)
    if n != 1:
        raise SystemExit("unitrules.xml: no <%s>" % tag)
    return new


def scale_cost(cost, k):
    """'5f/3m' (tens) times k: each part rounded down or up (at least 1), the combination whose
    total is nearest the target total (rounding each part alone left 4f/4t x 0.9 unchanged)."""
    parts = [(int(n), r) for n, r in re.findall(r"(\d+)([a-z])", cost)]
    target = sum(n for n, _ in parts) * k
    best = None
    for pick in range(1 << len(parts)):
        vals = [max(1, int(n * k) + (pick >> i & 1)) for i, (n, _) in enumerate(parts)]
        err = (abs(sum(vals) - target), sum(abs(v - n * k) for v, (n, _) in zip(vals, parts)))
        if best is None or err < best[0]:
            best = (err, vals)
    return "/".join("%d%s" % (v, r) for v, (_, r) in zip(best[1], parts))


def add_romanian_units(text):
    """The units of ROMANIAN_UNITS into their slots; Romania's bit off the rest of each line."""
    spans = [(m.start(1), m.end(1)) for m in re.finditer(r"<UNIT>(.*?)</UNIT>", text, re.S)]
    blocks = [text[s:e] for s, e in spans]
    report = []
    for slot, slot_name, generic, shown, target in ROMANIAN_UNITS:
        if field(blocks[slot], "NAME") != slot_name or field(blocks[slot], "WHERE") != "None":
            raise SystemExit("unitrules.xml: unit %d is not the free %s slot (taken by another mod?)" % (slot, slot_name))
        group = [i for i, b in enumerate(blocks) if i != slot and (
            (field(b, "NAME") == generic and field(b, "GRAFT").lower() == "none") or field(b, "GRAFT") == generic)]
        mine = [i for i in group if field(blocks[i], "TRIBE_MASK")[0] == "1"]
        if len(mine) != 1:
            raise SystemExit("%s: Romania has %d units of that line" % (generic, len(mine)))
        b = blocks[mine[0]]
        new = b
        cost = scale_cost(field(b, "COST"), target)
        power = sum(int(n) for n in re.findall(r"\d+", cost)) / sum(int(n) for n in re.findall(r"\d+", field(b, "COST")))
        for tag, value in (("NAME", slot_name), ("TYPENAME", slot_name), ("GRAFT", generic),
                           ("TRIBE_MASK", "1" + "0" * 24),
                           ("FLAGS", field(b, "FLAGS") + ("" if "y" in field(b, "FLAGS") else "y")),
                           ("ATTACK", str(round(int(field(b, "ATTACK")) * power))),
                           ("HITS", str(round(int(field(b, "HITS")) * power))),
                           ("COST", cost)):
            new = set_field(new, tag, value)
        if slot in OWN_ART:
            new = set_field(new, "GRAPH", OWN_ART[slot][0])
        blocks[slot] = new
        for i in group:
            blocks[i] = re.sub(r"<TRIBE_MASK>1", "<TRIBE_MASK>0", blocks[i])
        report.append("%-18s (slot %d %s, for %s): attack %s->%s, hits %s->%s, cost %s->%s, time %s->%s" % (
            shown, slot, slot_name, field(b, "NAME"), field(b, "ATTACK"), field(new, "ATTACK"), field(b, "HITS"),
            field(new, "HITS"), field(b, "COST"), field(new, "COST"), field(b, "JOB_TIME"), field(new, "JOB_TIME")))
    out, pos = [], 0
    for (s, e), b in zip(spans, blocks):
        out += [text[pos:s], b]
        pos = e
    out.append(text[pos:])
    return "".join(out), report


def build_typenames(game):
    """The slots' shown names (typenames.xml finds them by a hash of the internal name, which stays)."""
    text = read(base_of(game, "typenames.xml"))
    for _, slot_name, _, shown, *_ in ROMANIAN_UNITS:
        text, n = re.subn(r'(<STRING hash="\d+" needed="1">)%s(</STRING>)' % re.escape(slot_name),
                          lambda m: m.group(1) + shown + m.group(2), text)
        if n != 1:
            raise SystemExit("typenames.xml: %s found %d times" % (slot_name, n))
    return text


def build_help(game, rules):
    text = read(base_of(game, "help.xml"))
    bullets = "".join("\n         <BULLET/>" + line.format(*[constant(rules, c) for c in consts])
                      for consts, line in POWER)
    entry = '\t\t<ENTRY name="TRIBE%d">\n\t\t\t<STRING>%s%s\n         </STRING>\n\t\t</ENTRY>\n' % (
        ROMANIA, POWER_TITLE, bullets)
    text = re.sub(r'\t*<ENTRY name="TRIBE%d">.*?</ENTRY>\n' % ROMANIA, "", text, flags=re.S)
    end = text.index("</TRIBES>")
    line_start = text.rindex("\n", 0, end) + 1
    text = text[:line_start] + entry + text[line_start:]
    for family, like, *_ in OWN_ART.values():  # the unit's help is its graphics family's entry
        body = re.search(r'<ENTRY name="%s">(.*?)</ENTRY>' % like, text, re.S).group(1)
        text, n = re.subn(r'(<ENTRY name="%s">).*?(</ENTRY>)' % family, lambda m: m.group(1) + body + m.group(2),
                          text, flags=re.S)
        if n != 1:
            raise SystemExit("help.xml: %s found %d times" % (family, n))
    return text


def build_unit_graphics(game):
    text = read(base_of(game, "unit_graphics.xml"))
    for family, _, template, model, texture in OWN_ART.values():
        m = re.search(r'[ \t]*<UNIT name="%s".*?</UNIT>\r?\n' % re.escape(template), text, re.S)
        block = m.group(0).replace('name="%s"' % template, 'name="%s-DEFAULT-AGE0"' % family, 1)
        block = re.sub(r'model="[^"]*"', lambda _: 'model="%s"' % model, block, count=1)
        block = re.sub(r'texture="[^"]*"', lambda _: 'texture="%s"' % texture, block, count=1)
        old = list(re.finditer(r'[ \t]*<UNIT name="%s-[^"]*".*?</UNIT>\r?\n' % family, text, re.S))
        if not old:
            raise SystemExit("unit_graphics.xml: no %s entries" % family)
        text = text[:old[0].start()] + block + "".join(
            text[a.end():b.start()] for a, b in zip(old, old[1:])) + text[old[-1].end():]
    return text


def build_soundtypes(game):
    text = read(base_of(game, "soundtypes.xml"))
    for family, like, *_ in OWN_ART.values():
        body = re.search(r"  <%s>\r?\n(.*?)  </%s>" % (like, like), text, re.S).group(1)
        text, n = re.subn(r"(  <%s>\r?\n).*?(  </%s>)" % (family, family), lambda m: m.group(1) + body + m.group(2),
                          text, flags=re.S)
        if n != 1:
            raise SystemExit("soundtypes.xml: %s found %d times" % (family, n))
    return text


def slug(name):
    return re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")


def read_units():
    """source\\units.csv: (internal unit NAME, Romanian name) in order."""
    rows = []
    with open(os.path.join(SOURCE, "units.csv"), encoding="utf-8") as fh:
        for line in fh.read().splitlines()[1:]:
            if line.strip():
                name, ro, _history = line.split(";", 2)
                rows.append((name.strip(), ro.strip()))
    return rows


# Names from a later age on: (internal NAME, first age 1..8, name), [late] in romania_units.ini.
# From that age the unit keeps the game's own icon of its graphics tier (romania.c). The Supply
# Wagon is the game's truck from the Industrial Age (6), so it stops being an ox cart there (the
# user, 2026-10-29); DAC, the Brasov truck works, built the Romanian army's lorries.
LATE_UNITS = [("Supply Wagon", 6, "DAC Truck")]

# The game's own icon from an age on, the name staying Romanian: (internal NAME, first age),
# [game_icon] in romania_units.ini. romania.c puts the game's icon back once the player reaches
# that age, and Romania's while below it (the icon slots are per graphics tier, 6-8 together, so
# age 7 is followed by the player's age, not by a slot). The Citizen (the user, 2026-11-08): the
# game's modern Citizen in ages 7-8, the Razes before.
GAME_ICON_UNITS = [("Citizen", 7)]


def write_tga(img, path):
    """Uncompressed 32-bit, bottom-up, like the game's interface textures."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    a = np.asarray(img.convert("RGBA"))[::-1]
    with open(path, "wb") as fh:
        fh.write(bytes([0, 0, 2] + [0] * 9) + struct.pack("<HHBB", img.width, img.height, 32, 8))
        fh.write(a[..., [2, 1, 0, 3]].tobytes())


def fit(img, size):
    """Scales an icon to cover `size`, cropping the middle."""
    w, h = size
    s = max(w / img.width, h / img.height)
    img = img.convert("RGBA").resize((max(w, round(img.width * s)), max(h, round(img.height * s))), Image.LANCZOS)
    x, y = (img.width - w) // 2, (img.height - h) // 2
    return img.crop((x, y, x + w, y + h))


def build_units(game, out, unit_text):
    """romania_units.ini (next to mod_override\\, for the game folder), the icon atlases from
    icons\\<unit>.png and their IFACETEXLOAD lines in interface.xml. Returns the ini text."""
    names = set(re.findall(r"<NAME>(.*?)</NAME>", unit_text))
    rows = read_units()
    unknown = [n for n, _ in rows if n not in names]
    if unknown:
        raise SystemExit("units.csv: no such unit: " + ", ".join(unknown))
    icons = [(n, os.path.join(ICONS, slug(n) + ".png")) for n, _ in rows]
    icons = [(n, p) for n, p in icons if os.path.exists(p)]
    per = ATLAS_COLS * ATLAS_ROWS
    if len(icons) > 2 * per:
        raise SystemExit("more than %d icons" % (2 * per))
    lines = ["; Romania's unit names and icons, read by winmm.dll (camera-smooth\\src\\romania.c).",
             "; Built by romania\\tools\\build_data.py from romania\\source\\units.csv and romania\\icons\\.",
             "", "[names]"] + ["%s=%s" % r for r in rows] + \
        ["%s=%s" % (slot_name, shown) for _, slot_name, _, shown, *_ in ROMANIAN_UNITS] + ["", "[icons]"]
    atlases = []
    for i, (name, path) in enumerate(icons):
        a, c = divmod(i, per)
        if a == len(atlases):
            atlases.append((Image.new("RGBA", (512, 512), (0, 0, 0, 0)), Image.new("RGBA", (256, 256), (0, 0, 0, 0))))
        col, row = c % ATLAS_COLS, c // ATLAS_COLS
        src = Image.open(path)
        atlases[a][0].paste(fit(src, (61, 39)).filter(ImageFilter.UnsharpMask(1, 60, 2)), (col * 62, row * 40))
        atlases[a][1].paste(fit(src, (30, 19)), (col * 31, row * 20))
        lines.append("%s=%d,%d,%d" % (name, a, col, row))
        for _, slot_name, generic, *_ in ROMANIAN_UNITS:  # Romania's own unit of that line: the same icon
            if generic == name:
                lines.append("%s=%d,%d,%d" % (slot_name, a, col, row))
    lines += ["", "[late]"] + ["%s=%d,%s" % late for late in LATE_UNITS if late[0] in names]
    lines += ["", "[game_icon]"] + ["%s=%d" % g for g in GAME_ICON_UNITS if g[0] in names]
    tex = []
    for a, (full, half) in enumerate(atlases):
        write_tga(full, os.path.join(out, "art", "romania_units_%d.tga" % a))
        write_tga(half, os.path.join(out, "art", "romania_units_%d_half.tga" % a))
        tex.append('    <IFACETEXLOAD id="TEX_ROMANIA_%d" file="art\\romania_units_%d.tga" pixformat="PF_32"/>' % (a, a))
        tex.append('    <IFACETEXLOAD id="TEX_ROMANIA_%d_HALF" file="art\\romania_units_%d_half.tga" pixformat="PF_32"/>' % (a, a))
    base = os.path.join(ROOT, "ui-theme", "mod_override", "Data", "interface.xml")
    if not os.path.exists(base):
        base = os.path.join(game, "Data", "interface.xml")
    text = read(base)
    text = re.sub(r'[ \t]*<IFACETEXLOAD id="TEX_ROMANIA_[^>]*/>\r?\n', "", text)
    if tex:
        anchor = re.search(r'<IFACETEXLOAD id="TEX_UNITS_2_HALF[^>]*/>\r?\n', text)
        nl = newline(text)
        text = text[:anchor.end()] + nl.join(tex) + nl + text[anchor.end():]
        write(os.path.join(out, "Data", "interface.xml"), unique_rects(text))
    print("units: %d names, %d icons in %d atlas(es)" % (len(rows), len(icons), len(atlases)))
    return "\n".join(lines) + "\n"


UNIQUE_COLS, UNIQUE_ROWS = 3, 4  # romania.c's UNIQUE_RECTS = their product


def unique_rects(text):
    """The loading screen's unique units: 2 columns of 4 rects in the game, Romania has 11.
    3 per row, 4 rows, filled row by row (a nation with 4 uniques gets one row and a half)."""
    m = re.search(r'([ \t]*)<POSITIONING x="50" y="70" inc_x="150">(?:.*?</POSITIONING>){3}', text, re.S)
    if not m or m.group(0).count('name="unique_rect"') != 8:
        sys.exit("interface.xml: the loading screen's 8 unique_rects not found")
    ind, nl = m.group(1), newline(text)
    width = 408 // UNIQUE_COLS
    rect = ind + '    <IFACERECT name="unique_rect" width="%d" height="70"/>' % width
    row = [ind + '  <POSITIONING inc_x="%d">' % width] + [rect] * UNIQUE_COLS + [ind + '  </POSITIONING>']
    block = [ind + '<POSITIONING x="0" y="70" inc_y="70">'] + row * UNIQUE_ROWS + [ind + '</POSITIONING>']
    return text[:m.start()] + nl.join(block) + text[m.end():]


def main(game, out):
    data = os.path.join(out, "Data")
    rules = build_rules(game)
    write(os.path.join(data, "rules.xml"), rules)
    for name, tag in RULE_FILES.items():
        src = base_of(game, name)
        text, n, report = extend_masks(read(src), tag)
        if tag == "UNIT":
            text, own = add_romanian_units(text)
            report += own
        write(os.path.join(data, name), text)
        print("%s (from %s): Romania has %d types" % (name, os.path.relpath(src, ROOT) if src.startswith(ROOT) else src, n))
        for line in report:
            print("   ", line)
    write(os.path.join(data, "help.xml"), build_help(game, rules))
    write(os.path.join(data, "typenames.xml"), build_typenames(game))
    write(os.path.join(data, "unit_graphics.xml"), build_unit_graphics(game))
    write(os.path.join(data, "soundtypes.xml"), build_soundtypes(game))
    print("unit_graphics.xml, soundtypes.xml: %s" % ", ".join(
        "%s drawn with %s" % (a[0], os.path.basename(a[3])) for a in OWN_ART.values()))
    ini = build_units(game, out, read(os.path.join(data, "unitrules.xml")))
    write(os.path.join(out, "..", "romania_units.ini"), ini.replace("\n", "\r\n"))
    os.makedirs(os.path.join(out, "tribes"), exist_ok=True)
    shutil.copyfile(os.path.join(SOURCE, "romania.xml"), os.path.join(out, "tribes", "romania.xml"))
    print("rules.xml: 25 nations; help.xml: TRIBE%d; tribes\\romania.xml" % ROMANIA)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
