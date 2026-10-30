r"""How strong each nation's army is, age by age, against the average of all nations.

    python nation_strength.py <unitrules.xml> [nation]

For every military line (LINES: the generic units of each age, in order) and age, it finds the
unit each nation has: the newest tier of the line whose age has come, and of that tier the member
the nation's TRIBE_MASK allows (the generic unit, a unique unit or the same unit in other art,
which name the generic one in <GRAFT>). Works with 24- or 25-bit masks (Romania, the romania mod).

    power      = sqrt(attack / recharge * hit points)      how hard it hits and how long it lasts
    efficiency = power / cost                              per resource spent

Each nation's score in an age is the mean over the lines it has of its unit's value divided by the
mean of all nations' units of that line in that age (1.00 = average). Nation powers are left out
(cheaper or faster units, free units, bonus damage), and so are range, armor, speed, line of
sight and the engine's hidden bonuses (counterchart.txt): a first measure, from unitrules.xml only.
"""
import math
import re
import sys

TRIBES = ["Aztecs", "Maya", "Inca", "Bantu", "Nubians", "Greeks", "Romans", "Egyptians", "Turks", "Spanish",
          "French", "British", "Germans", "Russians", "Chinese", "Japanese", "Koreans", "Mongols", "Iroquois",
          "Lakota", "Americans", "Indians", "Dutch", "Persians", "Romania"]
AGES = ["none", "Classical Age", "Medieval Age", "Gunpowder Age", "Enlightenment Age", "Industrial Age",
        "Modern Age", "Information Age"]
AGE_NAMES = ["Ancient", "Classical", "Medieval", "Gunpowder", "Enlightenment", "Industrial", "Modern", "Information"]
LINES = {
    "light infantry": ["Slingers", "Javelineers", "Elite Javelineers", "Arquebusiers", "Musketeers", "Riflemen",
                       "Infantry", "Assault Infantry"],
    "heavy infantry, anti-tank": ["Hoplites", "Phalanx", "Pikemen", "Elite Pikemen", "Fusiliers", "Anti-tank Rifle",
                                  "Bazooka", "Anti-Tank Missile"],
    "foot archers": ["Bowmen", "Archers", "Crossbowmen"],
    "machine guns": ["Machine Gun", "Heavy Machine Gun", "Advanced Machine Gun"],
    "light cavalry": ["Light Horse", "Light Cavalry", "Elite Light Cavalry", "Hussar", "Armored Car",
                      "Armored Scout Car", "Armored Cavalry"],
    "mounted ranged": ["Horse Archer", "Heavy Horse Archer", "Dragoon", "Carabineer"],
    "heavy cavalry, tanks": ["Cataphract", "Knight", "Heavy Knight", "Cuirassier", "Light Tank", "Tank",
                             "Main Battle Tank"],
    "siege, artillery": ["Catapult", "Trebuchet", "Bombard", "Cannon", "Artillery", "Howitzer", "Rocket Artillery"],
    "fighters": ["Fighter", "Jet Fighter", "Advanced Fighter"],
    "bombers": ["Bomber", "Strategic Bomber"],
}


OUTLIER = 0.10


def num(s, d=0):
    m = re.search(r"-?\d+", s or "")
    return int(m.group()) if m else d


def cost(s):
    return sum(int(x) for x in re.findall(r"(\d+)[a-z]", s or ""))


def load(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    units = []
    for m in re.finditer(r"<UNIT>(.*?)</UNIT>", text, re.S):
        d = {k: v.strip() for k, v in re.findall(r"<([A-Z_0-9]+)>([^<]*)</\1>", m.group(1))}
        mask = d.get("TRIBE_MASK", "")
        bits = int(mask, 2) if mask else 0
        age = next((i for i, a in enumerate(AGES) if d.get("PREQ0", "").lower() == a.lower()), None)
        units.append(dict(name=d.get("NAME", ""), graft=d.get("GRAFT", "none"), bits=bits, age=age,
                          attack=num(d.get("ATTACK")), recharge=max(num(d.get("RECHARGE"), 1), 1),
                          hits=num(d.get("HITS")), cost=cost(d.get("COST"))))
    return units


def members(units, generic):
    """The generic unit (the first of that name) and every unit whose GRAFT names it."""
    return [u for u in units if (u["name"] == generic and u["graft"].lower() in ("none", generic.lower()))
            or u["graft"] == generic]


def unit_of(units, line, tribe, age):
    """The unit of `line` a nation fields in `age` (index), or None."""
    # In the file a generic unit's mask still has the nations whose own member replaces it;
    # Types::finalize_grafting takes the generic away from them when the game loads.
    best = None
    for generic in LINES[line]:
        group = [u for u in members(units, generic) if u["bits"] >> tribe & 1]
        own = [u for u in group if u["graft"] == generic]
        for u in own or group:
            if u["age"] is not None and u["age"] <= age and u["attack"] and u["hits"]:
                best = u
                break
    return best


def value(u):
    p = math.sqrt(u["attack"] / u["recharge"] * u["hits"])
    return p, p / u["cost"] if u["cost"] else None


def main(path, focus="Romania"):
    units = load(path)
    tribes = [t for t in range(len(TRIBES)) if any(u["bits"] >> t & 1 for u in units)]
    names = [TRIBES[t] for t in tribes]
    print("Nations: %d. Score 1.00 = the average of all nations in that age.\n" % len(tribes))
    totals = {t: [[], []] for t in tribes}
    for age in range(len(AGES)):
        scores = {t: [[], []] for t in tribes}
        detail, outliers = [], []
        for line in LINES:
            have = {t: unit_of(units, line, t, age) for t in tribes}
            vals = {t: value(u) for t, u in have.items() if u}
            if not vals:
                continue
            avg_p = sum(v[0] for v in vals.values()) / len(vals)
            effs = [v[1] for v in vals.values() if v[1]]
            avg_e = sum(effs) / len(effs) if effs else None
            for t, (p, e) in vals.items():
                scores[t][0].append(p / avg_p)
                if e and avg_e:
                    scores[t][1].append(e / avg_e)
            for t, (p, e) in vals.items():
                if abs(p / avg_p - 1) >= OUTLIER or (e and avg_e and abs(e / avg_e - 1) >= OUTLIER):
                    outliers.append("%s %s (%s) power %.2f, efficiency %s" % (
                        TRIBES[t], have[t]["name"], line, p / avg_p, "%.2f" % (e / avg_e) if e and avg_e else "-"))
            f = TRIBES.index(focus)
            if f in vals:
                u = have[f]
                p, e = vals[f]
                detail.append("    %-26s %-24s power %4.2f  efficiency %s" % (
                    line, u["name"], p / avg_p, "%4.2f" % (e / avg_e) if e and avg_e else "  - "))
        rows = []
        for t in tribes:
            p = sum(scores[t][0]) / len(scores[t][0]) if scores[t][0] else 0
            e = sum(scores[t][1]) / len(scores[t][1]) if scores[t][1] else 0
            rows.append((p, e, TRIBES[t]))
            totals[t][0].append(p)
            totals[t][1].append(e)
        rows.sort(reverse=True)
        rank = [r[2] for r in rows].index(focus) + 1 if focus in names else None
        mine = next((r for r in rows if r[2] == focus), None)
        print("%s Age: %s power %.2f, efficiency %.2f (rank %s of %d by power)" % (
            AGE_NAMES[age], focus, mine[0], mine[1], rank, len(rows)) if mine else AGE_NAMES[age])
        print("    strongest: " + ", ".join("%s %.2f" % (n, p) for p, e, n in rows[:3]) +
              "; weakest: " + ", ".join("%s %.2f" % (n, p) for p, e, n in rows[-3:]))
        print("\n".join(detail))
        if outliers:
            print("    units %d%% or more off their line's average:" % round(OUTLIER * 100))
            print("\n".join("      " + o for o in outliers))
    print("\nWhole game (mean of the 8 ages):")
    rows = sorted(((sum(v[0]) / len(v[0]), sum(v[1]) / len(v[1]), TRIBES[t]) for t, v in totals.items()), reverse=True)
    for p, e, n in rows:
        print("  %-10s power %.2f  efficiency %.2f%s" % (n, p, e, "   <--" if n == focus else ""))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "Romania")
