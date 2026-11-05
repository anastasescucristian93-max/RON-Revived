r"""The Romania section of the player guide (site\RON_Revived\index.html), made from the mod's data.

    python build_site.py <game folder>

For each of Romania's military units (the tiers of tools\nation_strength.py's LINES): its icon
(icons\, copied to site\RON_Revived\assets\romania\), its Romanian name and history line
(source\units.csv), and every stat with its value and the difference from the average of the
original game's 24 nations in that tier (the same reference as tools\stat_vs_average.py). The
section replaces what is between <!-- ROMANIA:BEGIN --> and <!-- ROMANIA:END --> in the page.
"""
import html
import os
import re
import shutil
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
sys.path.insert(0, os.path.join(ROOT, "tools"))
import nation_strength as ns  # noqa: E402
import stat_vs_average as sva  # noqa: E402
import build_data  # noqa: E402

SITE = os.path.join(ROOT, "site", "RON_Revived")
ASSETS = os.path.join(SITE, "assets", "romania")
ICONS = os.path.join(HERE, "..", "icons")
FLAG = os.path.join(ROOT, "ui-theme", "source", "flags", "cut", "romania.png")
PREVIEW = ["Phalanx", "Heavy Knight", "Cataphract", "Infantry", "Fighter", "Main Battle Tank"]  # the banner's strip
ROMANIA = ns.TRIBES.index("Romania")
RESOURCES = {"f": "Food", "t": "Timber", "m": "Metal", "g": "Wealth", "k": "Knowledge", "o": "Oil"}
LINE_NAMES = {
    "light infantry": "Light infantry", "heavy infantry, anti-tank": "Heavy infantry and anti-tank",
    "foot archers": "Foot archers", "machine guns": "Machine guns", "light cavalry": "Light cavalry",
    "mounted ranged": "Mounted ranged", "heavy cavalry, tanks": "Heavy cavalry and tanks",
    "siege, artillery": "Siege and artillery", "fighters": "Fighters", "bombers": "Bombers",
}
# (stat, label, True when less is better for the player)
SHOWN = [("ATTACK", "Attack", False), ("HITS", "HP", False), ("RECHARGE", "Reload", True),
         ("ARMOR", "Armor", False), ("RANGE", "Range", False), ("MOVES", "Speed", False),
         ("JOB_TIME", "Training", True), ("COST", "Cost", True)]  # the page puts the last one on its own row


def esc(s):
    return html.escape(s, quote=True)


def csv_rows():
    rows = {}
    with open(os.path.join(HERE, "..", "source", "units.csv"), encoding="utf-8") as fh:
        for line in fh.read().splitlines()[1:]:
            if line.strip():
                name, ro, history = line.split(";", 2)
                rows[name.strip()] = (ro.strip(), history.strip())
    return rows


def cost_text(cost):
    parts = re.findall(r"(\d+)([a-z])", cost or "")
    return " + ".join("%d %s" % (int(n) * 10, RESOURCES.get(r, r)) for n, r in parts)


# the guide's groups, in order; the army ones are tools\nation_strength.py's lines
CATS = ["army", "scouts", "support", "air", "navy", "leaders"]
CAT_LABELS = {"army": "Army", "scouts": "Scouts and militia", "support": "Support and civilians",
              "air": "Air and missiles", "navy": "Navy", "leaders": "Government leaders"}
OTHER = {
    "scouts": [("Scouts", ["Scout", "Explorer", "Commando", "Special Forces", "Elite Special Forces"]),
               ("Militia", ["Militia", "Minuteman", "Partisan"])],
    "support": [("Command", ["General", "Spy"]), ("Supply", ["Supply Wagon"]),
                ("Civilians", ["Citizen", "Scholar", "Caravan", "Merchant"])],
    "air": [("Anti-aircraft", ["Anti-Aircraft Gun", "Anti-Aircraft Battery", "Anti-Aircraft Missile"]),
            ("Aircraft", ["Biplane", "Fighter Bomber", "Jet Fighter Bomber", "Stealth Bomber", "Helicopter",
                          "Attack Helicopter"]),
            ("Missiles", ["V2 Rocket"])],
    "navy": [("Fishing and trade", ["Fishermen", "Merchant Fleet", "Modern Merchant Fleet"]),
             ("Transports", ["Transport Barge", "Transport Freighter"]),
             ("Light ships", ["Bark", "Dromon", "Caravel", "Corvette", "Sloop", "Destroyer", "Cruiser",
                              "Missile Cruiser"]),
             ("Fire ships", ["Fire Raft", "Heavy Fire Raft", "Fireship", "Heavy Fireship"]),
             ("Submarines", ["Submarine", "Attack Submarine"]),
             ("Heavy ships", ["Trireme", "Galley", "Carrack", "Frigate", "Dreadnought", "Battleship",
                              "Advanced Battleship"])],
    "leaders": [("Government leader", ["The Despot", "The Senator", "The Monarch", "The President"])],
}


def units_in_order(names):
    """(group, group label, line, generic unit) for every unit the guide shows."""
    for line, tiers in ns.LINES.items():
        cat = "air" if line in ("fighters", "bombers") else "army"
        for g in tiers:
            yield cat, CAT_LABELS[cat], LINE_NAMES[line], g
    for cat, lines in OTHER.items():
        for line, tiers in lines:
            for g in tiers:
                if g in names:
                    yield cat, CAT_LABELS[cat], line, g


def strip(units, total):
    """A row of icons (icon file, name) for a summary; the page's script shows as many as fit and
    turns the last place into "+N others" (the user: no cut-off icons)."""
    imgs = "".join('<img src="assets/romania/%s" alt="%s" title="%s" width="122" height="78" loading="lazy">' % (
        icon, esc(name), esc(name)) for icon, name in units if icon)
    return '<span class="ro-strip" data-total="%d">%s<span class="more"></span></span>' % (total, imgs)


def kind_of(slot_entry):
    if not slot_entry:
        return "standard", "Standard"
    return ("elite", "Elite") if slot_entry[4] > 1 else ("levy", "Cheaper")


def main(game):
    orig = os.path.join(game, "Data", "unitrules.xml")
    mod = os.path.join(HERE, "..", "mod_override", "Data", "unitrules.xml")
    o_units, m_units = ns.load(orig), ns.load(mod)
    o_stats, m_stats = sva.stats_of(orig), sva.stats_of(mod)
    m_raw = [dict(re.findall(r"<([A-Z_0-9]+)>([^<]*)</\1>", m.group(1)))
             for m in re.finditer(r"<UNIT>(.*?)</UNIT>", open(mod, encoding="utf-8").read(), re.S)]
    names = csv_rows()
    slots = {e[2]: e for e in build_data.ROMANIAN_UNITS}
    os.makedirs(ASSETS, exist_ok=True)
    cards = []
    done = set()
    for cat, cat_label, line, generic in units_in_order(names):
        if generic in done:
            continue
        done.add(generic)
        if True:
            idx = [i for i in (sva.tier_unit(o_units, generic, k) for k in range(24)) if i is not None]
            mine = sva.tier_unit(m_units, generic, ROMANIA)
            if not idx or mine is None:
                continue
            u = m_units[mine]
            ro, history = names.get(generic, (generic, ""))
            for late_name, late_age, late_shown in build_data.LATE_UNITS:  # a later name (romania_units.ini [late])
                if late_name == generic:
                    ro = "%s / %s" % (ro, late_shown)
                    history = "%s; from the %s Age, when it becomes a truck, the %s" % (
                        history, ns.AGE_NAMES[late_age - 1], late_shown)
            kind, kind_label = kind_of(slots.get(generic))
            src = os.path.join(ICONS, build_data.slug(generic) + ".png")
            icon = build_data.slug(generic) + ".jpg"
            if os.path.exists(src):
                Image.open(src).convert("RGB").resize((244, 156), Image.LANCZOS).save(
                    os.path.join(ASSETS, icon), quality=86, optimize=True)
                img = '<img src="assets/romania/%s" alt="" width="244" height="156" loading="lazy">' % icon
            else:
                img = '<div class="ro-noicon" aria-hidden="true"><span></span></div>'
            stats = []
            for tag, label, lower_better in SHOWN:
                avg = sva.reference([o_stats[i][tag] for i in idx], tag)
                v = m_stats[mine][tag]
                pct = (v / avg - 1) * 100 if avg >= 1 else 0
                shown_v = cost_text(m_raw[mine].get("COST")) if tag == "COST" else str(v)
                if tag == "RANGE" and v == 0:
                    shown_v = "melee"
                if abs(pct) < sva.SAME:
                    delta = '<span class="eq">=</span>'
                else:
                    good = (pct < 0) == lower_better
                    delta = '<span class="%s">%+.0f%%</span>' % ("up" if good else "down", pct)
                stats.append('<div><dt>%s</dt><dd><b>%s</b>%s</dd></div>' % (label, esc(shown_v), delta))
            age = ns.AGE_NAMES[u["age"]] + " Age" if u["age"] is not None else "With its government" \
                if cat == "leaders" else "With its research"
            cards.append((CATS.index(cat), u["age"] if u["age"] is not None else 9, """      <article class="ro-unit %s" data-cat="%s">
        %s
        <div class="ro-body">
          <div class="ro-head"><h4>%s</h4><span class="ro-kind %s">%s</span></div>
          <p class="ro-meta">%s &middot; %s &middot; in place of the %s</p>
          <p class="ro-hist">%s</p>
          <dl class="ro-stats">%s</dl>
        </div>
      </article>""" % (kind, cat, img, esc(ro), kind, kind_label, age, esc(line), esc(generic),
                       esc(history[:1].upper() + history[1:] + "."), "".join(stats)),
                          icon if os.path.exists(src) else None, ro, generic))
    cards.sort(key=lambda c: (c[1], c[0]))  # by age, then by kind
    present = sorted({c[0] for c in cards})
    buttons = ['<button type="button" data-cat="all" aria-pressed="true">All units</button>'] + [
        '<button type="button" data-cat="%s" aria-pressed="false">%s</button>' % (CATS[c], CAT_LABELS[CATS[c]])
        for c in present]
    # one folded block per age (the user: every age folds, all start closed)
    ages = []
    for a in sorted({c[1] for c in cards}):
        group = [c for c in cards if c[1] == a]
        title = ns.AGE_NAMES[a] + " Age" if a < len(ns.AGE_NAMES) else "With a government or a research"
        ages.append('    <details class="fold ro-age">\n      <summary><span class="ro-age-head">%s <small>%d units</small></span>%s</summary>\n'
                    '      <div class="ro-units">\n%s\n      </div>\n    </details>' % (
                        title, len(group), strip([(c[3], c[4]) for c in group], len(group)),
                        "\n".join(c[2] for c in group)))
    elite = [e for e in build_data.ROMANIAN_UNITS if e[4] > 1]
    levy = [e for e in build_data.ROMANIAN_UNITS if e[4] < 1]
    # the banner: the flag and a strip of the best-known units
    Image.open(FLAG).convert("RGBA").resize((192, 192), Image.LANCZOS).save(os.path.join(ASSETS, "flag.png"))
    by_generic = {c[5]: c for c in cards}
    thumbs = strip([(by_generic[g][3], by_generic[g][4]) for g in PREVIEW if g in by_generic], len(cards))
    section = SECTION.format(
        buttons="\n      ".join(buttons), ages="\n".join(ages), count=len(cards),
        own=len(build_data.ROMANIAN_UNITS), thumbs=thumbs,
        elite=", ".join(esc(e[3]) for e in elite), levy=", ".join(esc(e[3]) for e in levy))
    cards = [(c[0], c[2]) for c in cards]
    page_path = os.path.join(SITE, "index.html")
    page = open(page_path, encoding="utf-8").read()
    new, n = re.subn(r"<!-- ROMANIA:BEGIN -->.*?<!-- ROMANIA:END -->", lambda m: section, page, flags=re.S)
    if n != 1:
        raise SystemExit("index.html: no <!-- ROMANIA:BEGIN --> ... <!-- ROMANIA:END --> block")
    with open(page_path, "w", encoding="utf-8", newline="") as fh:
        fh.write(new)
    print("index.html: Romania, %d units (%d with their own icon)" % (len(cards), sum('assets/romania/' in c for _, c in cards)))


SECTION = """<!-- ROMANIA:BEGIN -->
<section id="romania">
  <h2>Romania</h2>
  <p class="sub">A 25th nation, with its own flag, leaders, cities, nation power, unit names from Romanian history, its own unit icons and George Enescu's music.</p>

  <details class="fold ro-fold">
  <summary class="ro-banner">
    <img class="ro-flag" src="assets/romania/flag.png" alt="" width="96" height="96">
    <span class="ro-banner-text">
      <span class="ro-banner-title">Romania, the 25th nation</span>
      <span class="ro-banner-sub">The Power of the Carpathians &middot; {count} units named from Romanian history &middot; {own} units of its own &middot; Enescu's music</span>
      {thumbs}
    </span>
    <span class="ro-cta"><span class="when-closed">Show the nation and its units</span><span class="when-open">Hide</span></span>
  </summary>
  <div class="features">
    <div class="feature"><h4>The Power of the Carpathians</h4><p>Attrition damage to enemy units in your territory +100%, and the attrition research is free. +20% Oil gathering. Forts and Towers 25% cheaper; Forts 50% faster to build, +3 National Borders and their research free. The Militia, Minuteman and Partisan upgrades free.</p></div>
    <div class="feature"><h4>Quality against quantity</h4><p>Some Romanian units are an elite that costs more ({elite}); others are levies, reservists and older machines that cost less ({levy}). The price follows the strength: every one of them gives the same strength for its resources as the usual unit.</p></div>
    <div class="feature"><h4>Leaders, cities, music</h4><p>Computer players are named after Burebista, Decebalus, Mircea the Elder, Vlad the Impaler, Stephen the Great, Michael the Brave, Queen Marie and others; cities from Bucuresti and Sarmizegetusa on. Enescu's Romanian Rhapsodies play among the game's tracks (the recording has to be added by hand, see the mod's README).</p></div>
  </div>

  <h3>The units</h3>
  <p class="prose">Every unit Romania fields ({count} in all), by kind and age. Each number is compared with the <b>average of the 24 original nations</b> for the same unit in the original game (their own unique units included): <span class="ro-legend"><span class="up">green</span> is better for you, <span class="down">red</span> is worse</span>; for Reload, Cost and Training less is better. The average is rounded to whole numbers (costs to tens, as the game keeps them) and differences under 3% count as equal, so only real differences show.</p>
  <div class="ro-filter" role="group" aria-label="Show one kind of unit">
      {buttons}
  </div>
{ages}
  </details>
  <script>
  (function () {{
    // icon rows: as many icons as fit, the last place "+N others" when some are left out
    function fit(s) {{
      var imgs = s.querySelectorAll('img'), more = s.querySelector('.more'), total = +s.dataset.total;
      if (!s.clientWidth) return;
      var step = 98, cap = Math.max(1, Math.floor((s.clientWidth + 6) / step)), shown = Math.min(imgs.length, cap);
      if (total > shown && shown >= cap) shown = cap - 1;
      imgs.forEach(function (im, i) {{ im.hidden = i >= shown; }});
      more.hidden = total <= shown;
      more.textContent = '+' + (total - shown) + ' others';
    }}
    function fitAll() {{ document.querySelectorAll('.ro-strip').forEach(fit); }}
    fitAll();
    window.addEventListener('resize', fitAll);
    document.querySelectorAll('.ro-fold, .ro-age').forEach(function (d) {{ d.addEventListener('toggle', fitAll); }});
  }})();
  (function () {{
    var f = document.querySelector('.ro-filter');
    if (!f) return;
    f.addEventListener('click', function (e) {{
      var b = e.target.closest('button');
      if (!b) return;
      f.querySelectorAll('button').forEach(function (x) {{ x.setAttribute('aria-pressed', x === b ? 'true' : 'false'); }});
      document.querySelectorAll('.ro-age').forEach(function (age) {{
        var shown = 0;
        age.querySelectorAll('.ro-unit').forEach(function (u) {{
          u.hidden = b.dataset.cat !== 'all' && u.dataset.cat !== b.dataset.cat;
          if (!u.hidden) shown++;
        }});
        age.hidden = shown === 0;
        age.querySelector('summary small').textContent = shown + (shown === 1 ? ' unit' : ' units');
      }});
    }});
  }})();
  </script>
</section>
<!-- ROMANIA:END -->"""

if __name__ == "__main__":
    main(sys.argv[1])
