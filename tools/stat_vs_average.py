r"""Each stat of a nation's units in percent against the average of the original game's 24 nations.

    python stat_vs_average.py <original unitrules.xml> <modded unitrules.xml> [nation] [--all]

For every tier of every military line (nation_strength.LINES) the average of each stat is taken
over the unit each of the 24 nations has in that tier in the original game (its unique unit or
the generic one, after grafting). The nation's unit of that tier in the modded file is then shown
against it. Without --all only the tiers where the nation's unit differs from the generic one.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nation_strength as ns

STATS = [("ATTACK", "attack"), ("RECHARGE", "time between attacks"), ("HITS", "hit points"), ("ARMOR", "armor"),
         ("RANGE", "range"), ("MOVES", "speed"), ("COST", "cost"), ("JOB_TIME", "training time")]


def stats_of(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    out = []
    for m in re.finditer(r"<UNIT>(.*?)</UNIT>", text, re.S):
        d = {k: v.strip() for k, v in re.findall(r"<([A-Z_0-9]+)>([^<]*)</\1>", m.group(1))}
        s = {}
        for tag, _ in STATS:
            v = d.get(tag, "")
            if tag == "COST":
                s[tag] = ns.cost(v) * 10
            elif tag == "RANGE":
                nums = re.findall(r"\d+", v)
                s[tag] = int(nums[-1]) if nums else 0
            else:
                s[tag] = ns.num(v)
        out.append(s)
    return out


SAME = 3  # differences under this many percent are shown as "=" (noise of the average)


def reference(values, tag):
    """The average of a stat, rounded as the game keeps it: whole numbers, costs in tens (the
    user, 2026-10-26: no +-1-2 % that only come from other nations' units in the average)."""
    avg = sum(values) / len(values)
    return round(avg / 10) * 10 if tag == "COST" else round(avg)


def tier_unit(units, generic, tribe):
    group = [i for i, u in enumerate(units) if u["bits"] >> tribe & 1 and (
        (u["name"] == generic and u["graft"].lower() in ("none", generic.lower())) or u["graft"] == generic)]
    own = [i for i in group if units[i]["graft"] == generic]
    return (own or group or [None])[0]


def main(orig, mod, nation="Romania", show_all=False):
    o_units, m_units = ns.load(orig), ns.load(mod)
    o_stats, m_stats = stats_of(orig), stats_of(mod)
    t = ns.TRIBES.index(nation)
    for line, tiers in ns.LINES.items():
        for generic in tiers:
            idx = [tier_unit(o_units, generic, k) for k in range(24)]
            idx = [i for i in idx if i is not None]
            mine = tier_unit(m_units, generic, t)
            if not idx or mine is None:
                continue
            u = m_units[mine]
            if not show_all and u["name"] == generic and u["graft"].lower() in ("none", generic.lower()):
                continue
            age = ns.AGE_NAMES[u["age"]] if u["age"] is not None else "?"
            cells = []
            for tag, label in STATS:
                avg = reference([o_stats[i][tag] for i in idx], tag)
                v = m_stats[mine][tag]
                pct = (v / avg - 1) * 100 if avg >= 1 else 0  # melee: a few units of the tier have range 1
                cells.append(pct)
            if MD:
                shown = NAMES.get(u["name"], u["name"])
                print("| %s | %s | %s | %s |" % (shown, age, generic, " | ".join(
                    ("**%+.0f%%**" % p if abs(p) >= 5 else "%+.0f%%" % p) if abs(p) >= SAME else "=" for p in cells)))
            else:
                print("%-22s %-13s (%s, %s)\n    %s" % (u["name"], age, line, generic, ", ".join(
                    "%s %+.0f%%" % (label, p) if abs(p) >= SAME else "%s =" % label for (_, label), p in zip(STATS, cells))))


MD = "--md" in sys.argv  # a Markdown table row per unit
NAMES = {}  # --names=<ini>: shown names from romania_units.ini [names]
for a in sys.argv:
    if a.startswith("--names="):
        sec = open(a[8:], encoding="utf-8").read().split("[names]")[1].split("[")[0]
        NAMES = dict(l.split("=", 1) for l in sec.splitlines() if "=" in l)

if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    main(args[0], args[1], args[2] if len(args) > 2 else "Romania", "--all" in sys.argv)
