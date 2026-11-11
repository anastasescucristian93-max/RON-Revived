r"""Builds camera-smooth\src\unitbans_data.h, the table of the game setup's Units window
(camera-smooth\src\unitbans.c): which units can be turned off, in which column they stand,
and which unit each one is upgraded from.

    python gen_unitbans.py [unitrules.xml]

The default source is the last unitrules.xml of the mod chain (romania's, which has the 25th
nation's units). Units are named by their internal NAME: several unit types with the same NAME
(the Tank in each nation's art) are one entry.

- Standard units: the columns below (one upgrade line each, some short lines together),
  generic units only. Every trainable generic unit that is not left out on purpose must be in a
  column; the script stops when one is missing, so a new unit is not forgotten.
- Unique units: one column per nation, every unit flagged 'y' that the nation can train, in the
  order of unitrules.xml (line by line, Age by Age).
- Left out: civilians (Citizen, Scholar, Caravan, Merchant, the fishing and trade fleets,
  transports), the Senate's leaders, nuclear weapons (the Nukes slider has them), units no
  building trains (WHERE None / disable) or no nation has (all-zero TRIBE_MASK).
- Icons: the unit's icon cell (unit_graphics.xml "<GRAPH>-DEFAULT-AGE0", else the first entry of
  its GRAPH): page 0..2 = art\iface_units_01..03.tga; Romania's own units page 3 =
  art\romania_units_0.tga (romania_units.ini [icons]). The window draws icons, not names.
- FROM: the unit a unit is upgraded from. A unit turned off takes every later upgrade with it
  (the game offers an upgrade only after the one before), so the DLL turns those off too.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(ROOT, "romania", "mod_override", "Data", "unitrules.xml")
GRAPHICS = os.path.join(ROOT, "romania", "mod_override", "Data", "unit_graphics.xml")  # last of the chain
ROMANIA_INI = os.path.join(ROOT, "romania", "romania_units.ini")
OUT = os.path.join(ROOT, "camera-smooth", "src", "unitbans_data.h")

TRIBES = 25  # with Romania (tribe 24); TRIBE_MASK is read high bit first

# The standard columns, by building. Names are unitrules NAMEs.
COLUMNS = [
    ["Hoplites", "Phalanx", "Pikemen", "Elite Pikemen", "Fusiliers", "Anti-tank Rifle", "Bazooka",
     "Anti-Tank Missile"],
    ["Slingers", "Javelineers", "Elite Javelineers", "Arquebusiers", "Musketeers", "Riflemen", "Infantry",
     "Assault Infantry"],
    ["Bowmen", "Archers", "Crossbowmen", "Heavy Archers"],
    ["Scout", "Explorer", "Commando", "Special Forces", "Elite Special Forces"],
    ["Machine Gun", "Heavy Machine Gun", "Advanced Machine Gun", "Flamethrower"],
    ["Militia", "Minuteman", "Partisan"],
    ["Cataphract", "Knight", "Heavy Knight", "Cuirassier", "Light Tank", "Tank", "Main Battle Tank"],
    ["Horse Archer", "Heavy Horse Archer", "Dragoon", "Carabineer", "Armored Car", "Armored Scout Car",
     "Armored Cavalry"],
    ["Light Horse", "Light Cavalry", "Elite Light Cavalry", "Hussar"],
    ["Catapult", "Trebuchet", "Bombard", "Cannon", "Artillery", "Howitzer", "Rocket Artillery"],
    ["Anti-Aircraft Gun", "Anti-Aircraft Battery", "Anti-Aircraft Missile"],
    ["Biplane", "Fighter", "Jet Fighter", "Advanced Fighter"],
    ["Bomber", "Strategic Bomber", "Stealth Bomber"],
    ["Helicopter", "Attack Helicopter"],
    ["V2 Rocket", "Cruise Missile", "SAS"],  # SAS: nuke-fx's Interceptor
    ["Trireme", "Galley", "Carrack", "Frigate", "Man o' War", "Dreadnought", "Battleship",
     "Advanced Battleship"],
    ["Bark", "Dromon", "Caravel", "Corvette", "Sloop", "Destroyer", "Cruiser", "Missile Cruiser"],
    ["Fire Raft", "Heavy Fire Raft", "Fireship", "Heavy Fireship", "Submarine", "Attack Submarine"],
    ["Bomb Vessel", "Bomb Ketch", "Aircraft Carrier"],
    ["General", "Spy", "Supply Wagon"],
]

LEFT_OUT = {"Citizen", "Scholar", "Caravan", "Merchant", "Armed Caravan", "Armed Merchant", "Fishermen",
            "Merchant Fleet", "Modern Merchant Fleet", "Transport Barge", "Transport Galleon",
            "Transport Freighter", "Nuclear Missile", "Nuclear ICBM"}


def parse(path):
    text = open(path, encoding="latin-1").read()
    units = []
    for m in re.finditer(r"<UNIT>(.*?)</UNIT>", text, re.S):
        d = dict(re.findall(r"<([A-Z_0-9]+)>([^<]*)</\1>", m.group(1)))
        d = {k: v.strip() for k, v in d.items()}
        units.append(d)
    return units


def trainable(u):
    mask = u.get("TRIBE_MASK", "")
    return (u.get("WHERE", "None") not in ("None", "disable", "") and "1" in mask
            and not u.get("PREQ1", "").lower() == "disable" and u["NAME"] not in LEFT_OUT)


def tribes_of(u):
    mask = u.get("TRIBE_MASK", "")
    return [len(mask) - 1 - i for i, c in enumerate(mask) if c == "1" and len(mask) - 1 - i < TRIBES]


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else SRC
    units = [u for u in parse(src) if trainable(u)]
    by_name = {}
    for u in units:
        by_name.setdefault(u["NAME"], []).append(u)
    unique = {n for n, us in by_name.items() if all("y" in u.get("FLAGS", "") for u in us)}

    # entries: standard columns first, then the unique units in file order
    names = []
    for col in COLUMNS:
        for n in col:
            if n not in by_name:
                raise SystemExit("column unit not trainable or missing: %s" % n)
            if n in unique:
                raise SystemExit("unique unit in a standard column: %s" % n)
            if n in names:
                raise SystemExit("unit in two columns: %s" % n)
            names.append(n)
    missing = sorted(n for n in by_name if n not in unique and n not in names)
    if missing:
        raise SystemExit("generic units in no column: %s" % ", ".join(missing))
    for u in units:
        if u["NAME"] in unique and u["NAME"] not in names:
            names.append(u["NAME"])
    index = {n.lower(): i for i, n in enumerate(names)}

    def from_of(n):
        f = by_name[n][0].get("FROM", "none")
        return index.get(f.lower(), -1)

    nations = [[] for _ in range(TRIBES)]
    for u in units:
        if u["NAME"] not in unique:
            continue
        for t in tribes_of(u):
            i = index[u["NAME"].lower()]
            if i not in nations[t]:
                nations[t].append(i)
    # a nation's units by building, then line by line, Age by Age
    buildings = ["Barracks", "Tower", "Stable", "Siege Factory", "Airbase", "Missile Silo", "Dock", "Fort"]
    ages = ["none", "Classical Age", "Medieval Age", "Gunpowder Age", "Enlightenment Age", "Industrial Age",
            "Modern Age", "Information Age"]

    def age_of(i):
        p = by_name[names[i]][0].get("PREQ0", "none").lower()
        return next((k for k, a in enumerate(ages) if a.lower() == p), 0)

    def building_of(i):
        w = by_name[names[i]][0].get("WHERE", "")
        return buildings.index(w) if w in buildings else len(buildings)

    for t in range(TRIBES):
        mine = nations[t]
        members = set(mine)

        def root(i):
            while from_of(names[i]) in members:
                i = from_of(names[i])
            return i

        first, pos = {}, {i: k for k, i in enumerate(mine)}
        for i in mine:
            first.setdefault(root(i), len(first))
        mine.sort(key=lambda i: (building_of(root(i)), age_of(root(i)), first[root(i)], age_of(i), pos[i]))
    only_romania = [all(tribes_of(u) == [24] for u in by_name[n]) for n in names]

    gtext = open(GRAPHICS, encoding="latin-1").read()
    cells = {}
    for m in re.finditer(r'<UNIT name="([^"]+)"[^>]*?tex_id="([^"]*)"[^>]*?tex_col="([^"]*)"[^>]*?tex_row="([^"]*)"', gtext):
        name, tex, col, row = m.groups()
        if tex.startswith("TEX_UNITS_"):
            cells.setdefault(name.upper(), (int(tex[-1]), int(col), int(row)))
    ro_icons = {}
    if os.path.exists(ROMANIA_INI):
        sect = None
        for line in open(ROMANIA_INI, encoding="latin-1"):
            line = line.strip()
            if line.startswith("["):
                sect = line
            elif sect == "[icons]" and "=" in line:
                k, v = line.split("=", 1)
                a, c, r = (int(x) for x in v.split(","))
                if a == 0:
                    ro_icons[k.strip().lower()] = (3, c, r)

    def icon_of(i):
        n = names[i]
        if only_romania[i] and n.lower() in ro_icons:
            return ro_icons[n.lower()]
        for u in by_name[n]:
            g = u.get("GRAPH", "").upper()
            if g + "-DEFAULT-AGE0" in cells:
                return cells[g + "-DEFAULT-AGE0"]
            for k, v in cells.items():
                if k.startswith(g + "-"):
                    return v
        return (-1, 0, 0)

    icons = [icon_of(i) for i in range(len(names))]
    no_icon = [names[i] for i in range(len(names)) if icons[i][0] < 0]
    if no_icon:
        print("no icon:", ", ".join(no_icon))

    n = len(names)
    sig = 0
    for name in names:
        for ch in name.encode("latin-1"):
            sig = (sig * 31 + ch) & 0xFFFFFFFF
    out = []
    out.append("/* Made by camera-smooth\\tools\\gen_unitbans.py from %s; do not edit. */" %
               os.path.relpath(src, ROOT).replace("/", "\\"))
    out.append("#define UB_ENTRIES %d" % n)
    out.append("#define UB_STD_COLS %d" % len(COLUMNS))
    out.append("#define UB_NATIONS %d" % TRIBES)
    out.append("#define UB_SIG 0x%08Xu /* the entries' names, so a saved list of another table is not read */" % sig)
    out.append("static const char *const k_ub_name[UB_ENTRIES] = {")
    for i, name in enumerate(names):
        out.append('    "%s", /* %d */' % (name.replace("\\", "\\\\").replace('"', '\\"'), i))
    out.append("};")
    out.append("/* the entry it is upgraded from, -1 none */")
    out.append("static const short k_ub_from[UB_ENTRIES] = {")
    out.append("    " + ", ".join(str(from_of(nm)) for nm in names))
    out.append("};")
    out.append("/* 1: only Romania has it (its shown name comes from romania_units.ini) */")
    out.append("/* icon: page (0..2 iface_units_01..03, 3 romania_units_0; -1 none), column, row */")
    out.append("static const signed char k_ub_icon[UB_ENTRIES][3] = {")
    out.append("    " + ", ".join("{ %d, %d, %d }" % ic for ic in icons))
    out.append("};")
    out.append("static const unsigned char k_ub_romania_only[UB_ENTRIES] = {")
    out.append("    " + ", ".join("1" if r else "0" for r in only_romania))
    out.append("};")
    out.append("/* standard columns: entry indexes, -1 ends a column */")
    out.append("static const short k_ub_std[] = {")
    for col in COLUMNS:
        out.append("    " + ", ".join(str(index[c.lower()]) for c in col) + ", -1,")
    out.append("};")
    out.append("/* unique units per nation (tribe index), -1 ends a nation */")
    out.append("static const short k_ub_nation[] = {")
    tr = ["Aztecs", "Maya", "Inca", "Bantu", "Nubians", "Greeks", "Romans", "Egyptians", "Turks", "Spanish",
          "French", "British", "Germans", "Russians", "Chinese", "Japanese", "Koreans", "Mongols", "Iroquois",
          "Lakota", "Americans", "Indians", "Dutch", "Persians", "Romania"]
    for t in range(TRIBES):
        out.append("    " + "".join("%d, " % i for i in nations[t]) + "-1, /* %d %s */" % (t, tr[t]))
    out.append("};")
    open(OUT, "w", encoding="ascii", newline="\n").write("\n".join(out) + "\n")
    longest = max(len(c) for c in COLUMNS)
    print("%s: %d entries (%d standard in %d columns, longest %d; %d unique), signature %08X" %
          (OUT, n, sum(len(c) for c in COLUMNS), len(COLUMNS), longest, n - sum(len(c) for c in COLUMNS), sig))
    for t in range(TRIBES):
        print("  %-9s %s" % (tr[t], ", ".join(names[i] for i in nations[t])))


if __name__ == "__main__":
    main()
