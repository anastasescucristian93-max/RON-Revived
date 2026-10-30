"""Turn the player guide (site/RON_Revived/index.html) into GitHub wiki pages.

    python site/make_wiki.py <wiki clone folder>

Every top-level <section> of the guide becomes one page, plus Home, _Sidebar and _Footer.
Pictures are not copied: the pages point at site/RON_Revived/assets in the main repository,
so push the main repository first. Needs: pip install markdownify (brings beautifulsoup4).
"""
import re
import sys
from pathlib import Path

from bs4 import BeautifulSoup
from markdownify import markdownify
from PIL import Image

REPO = "https://github.com/anastasescucristian93-max/RON-Revived"
ASSETS = "https://raw.githubusercontent.com/anastasescucristian93-max/RON-Revived/main/site/RON_Revived/"
GUIDE = Path(__file__).parent / "RON_Revived" / "index.html"
TILE_SCALE = 0.5  # building pictures: the widest (a 581 px Large City) fills a third of the page

# Sidebar groups, as in the guide's own menu; ids are the guide's <section> ids
GROUPS = [
    ("Getting started", ["install"]),
    ("Features", ["features", "ranks", "assistant", "districts", "airdrop", "fortification",
                  "survival", "romania", "languages", "look", "graphics", "buildings"]),
    ("Reference", ["settings", "multiplayer", "issues", "updates"]),
]
TAG_WORDS = {"new": "New", "fix": "Fix", "chg": "Change", "rule": "Rule"}
# Sentences about the guide's own clickable page, which mean nothing in the wiki
GUIDE_ONLY = [
    r" ?Each card says in two lines what it does; click it for the details\.",
    r" ?Drag the line over a picture to compare\.",
    r" ?Pick a nation to see its buildings\.",
    r" ?Scroll sideways for all of them\.",
    r" ?Click a day to open or close it\.",
    r" ?Try it on the (\[)?(live )?launcher[^.]*\.",
    r"Show the nation and its unitsHide",
]

raw = []  # HTML blocks markdownify must not touch, put back after the conversion


def keep(html):
    raw.append(html)
    return f"@@RAW{len(raw) - 1}@@"


def asset(src):
    return ASSETS + src if src.startswith("assets/") else src


def slug(text):
    return re.sub(r"[^a-z0-9 -]", "", text.lower()).strip().replace(" ", "-")


def heading_text(h):
    for t in h.find_all(class_="tag"):
        t.decompose()
    return h.get_text(" ", strip=True)


def page_name(title):
    return re.sub(r"[^\w -]", "", title).replace(" ", "-")


def esc(text):
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def unit_table(units):
    """The Romanian unit cards of one age, as one table."""
    stats = [dt.get_text(strip=True) for dt in units[0].find_all("dt")]
    rows = ["| Unit | " + " | ".join(stats) + " |", "|---|" + "---|" * len(stats)]
    for u in units:
        img = u.find("img", recursive=False)
        icon = f'<img src="{asset(img["src"])}" width="72"><br>' if img else ""
        name = u.find("h4").get_text(strip=True)
        kind = u.find(class_="ro-kind").get_text(strip=True)
        meta = u.find(class_="ro-meta").get_text(strip=True)
        cell = f"{icon}**{name}** ({kind})<br><sub>{meta}</sub>"
        values = []
        for dd in u.find_all("dd"):
            v = f"**{dd.find('b').get_text(strip=True)}**"
            diff = dd.find(class_=["up", "down"])
            if diff:
                v += (" 🟢" if "up" in diff["class"] else " 🔴") + diff.get_text(strip=True)
            values.append(v)
        rows.append(f"| {cell} | " + " | ".join(values) + " |")
    return "\n".join(rows)


def gallery(div):
    """Picture tiles as a three-column HTML table (markdown has no grid). The pictures are the
    sprites at 1:1, so one scale for all keeps the buildings' sizes as in the game; they stand
    on a common ground line with the captions in a row below."""
    pics, caps = [], []
    for fig in div.find_all("figure"):
        img = fig.find("img")
        width = round(Image.open(GUIDE.parent / img["src"].removeprefix(ASSETS)).width * TILE_SCALE)
        cap = fig.find("figcaption")
        title = cap.find("b").extract().get_text(strip=True) if cap and cap.find("b") else ""
        text = cap.get_text(" ", strip=True) if cap else ""
        pics.append(f'<td width="33%" align="center" valign="bottom"><img src="{img["src"]}" width="{width}"></td>')
        caps.append(f'<td valign="top"><b>{esc(title)}</b><br>{esc(text)}</td>')
    rows = ["<tr>" + "".join(pics[i:i + 3]) + "</tr><tr>" + "".join(caps[i:i + 3]) + "</tr>"
            for i in range(0, len(pics), 3)]
    return "<table>" + "".join(rows) + "</table>"


def before_after(cmp):
    old, new = cmp.find_all("img")[:2]
    return ("| Before | After |\n|---|---|\n"
            f'| <img src="{asset(old["src"])}" width="400"> | <img src="{asset(new["src"])}" width="400"> |')


def convert(node, links):
    for t in node.find_all(["script", "style", "button", "input", "canvas"]):
        t.decompose()
    for t in node.select(".cmp-line, .cmp-tag, .digest, .ro-strip, .ro-noicon, a.btn"):
        t.decompose()
    for t in node.find_all(class_="tag"):
        kind = [c for c in t["class"] if c != "tag"]
        word = t.get_text(strip=True)
        if "test" not in kind and t.find_parent(["h3", "h4"]):
            t.decompose()
            continue
        t.replace_with(BeautifulSoup(f" <em>({word.lower()})</em>" if "test" in kind
                                     else f"<strong>{TAG_WORDS.get(kind[0], word)}:</strong> ", "html.parser"))
    for s in node.select("details.day > summary"):
        s.name = "h3"
    for t in node.find_all("small"):
        t.insert_before(" · ")
    for t in node.find_all("img"):
        t["src"] = asset(t["src"])
    for a in node.find_all("a", href=True):
        if a["href"].startswith("#"):
            a["href"] = links.get(a["href"][1:], "Home")
    for d in node.find_all("details", class_="ro-age"):
        summary = d.find("summary").get_text(" ", strip=True)
        d.replace_with(BeautifulSoup(f"<h4>{esc(summary)}</h4><p>{keep(unit_table(d.find_all('article')))}</p>",
                                     "html.parser"))
    for div in {id(f.parent): f.parent for f in node.select("figure.tile")}.values():
        div.replace_with(BeautifulSoup(f"<p>{keep(gallery(div))}</p>", "html.parser"))
    for cmp in node.find_all("div", class_="cmp"):
        cmp.replace_with(BeautifulSoup(f"<p>{keep(before_after(cmp))}</p>", "html.parser"))
    for cap in node.find_all("figcaption"):
        cap.name = "p"
        cap.wrap(BeautifulSoup("<em></em>", "html.parser").em)
    md = markdownify(str(node), heading_style="ATX", bullets="-")
    md = re.sub(r"@@RAW(\d+)@@", lambda m: raw[int(m.group(1))], md)
    for pattern in GUIDE_ONLY:
        md = re.sub(pattern, "", md)
    md = md.replace(")[", ") · [")  # link buttons that sat side by side
    return re.sub(r"\n{3,}", "\n\n", md).strip() + "\n"


def main(out):
    soup = BeautifulSoup(GUIDE.read_text(encoding="utf-8"), "html.parser")
    sections = {s["id"]: s for s in soup.find_all("section", id=True) if not s.find_parent("section")}
    titles = {i: heading_text(s.find("h2")) for i, s in sections.items()}

    # The feature tabs become headings of their own
    for panel in soup.select(".feat-panel"):
        tab = soup.find(attrs={"aria-controls": panel["id"]})
        panel.insert(0, BeautifulSoup(f"<h3>{esc(tab.get_text(strip=True))}</h3>", "html.parser"))

    # Where every #id of the guide lives in the wiki
    links = {i: page_name(t) for i, t in titles.items()}
    for i, s in sections.items():
        for sub in s.find_all(id=True):
            h = sub.find(["h3", "h4"])
            text = heading_text(h) if h else ""
            links[sub["id"]] = links[i] + ("#" + slug(text) if text else "")

    out.mkdir(parents=True, exist_ok=True)
    download = f"**[Download the latest version]({REPO}/releases/latest)**"
    for i, s in sections.items():
        s.find("h2").decompose()
        body = convert(s, links)
        if i == "install":
            body = download + "\n\n" + body
        (out / f"{links[i]}.md").write_text(body, encoding="utf-8")

    hero = soup.find("header", class_="hero")
    lede = convert(hero.find(class_="lede"), links)
    img = hero.find("img")
    pages = "\n".join(f"- **{g}:** " + " · ".join(f"[{titles[i]}]({links[i]})" for i in ids) for g, ids in GROUPS)
    (out / "Home.md").write_text(
        f'<img src="{asset(img["src"])}" align="right" width="320">\n\n'
        "A mod pack for **Rise of Nations: Extended Edition** (Steam). "
        "The game's files are never modified, and one click installs or removes it all.\n\n"
        f"{lede}\n{download}, extract it and double-click `install.bat`. Details: [Install](Install).\n\n"
        f"## Pages\n\n{pages}\n", encoding="utf-8")

    side = ["**[Home](Home)**", ""]
    for g, ids in GROUPS:
        side += [f"**{g}**", ""] + [f"- [{titles[i]}]({links[i]})" for i in ids] + [""]
    (out / "_Sidebar.md").write_text("\n".join(side), encoding="utf-8")
    (out / "_Footer.md").write_text(
        f"[Download]({REPO}/releases/latest) · [Reviews]({REPO}/discussions/1) · "
        f"[Report a bug]({REPO}/issues/new?template=bug_report.yml) · "
        f"[Suggest an idea]({REPO}/issues/new?template=idea.yml) · Liked it? Give the repository a star ⭐\n",
        encoding="utf-8")
    print(f"{len(sections) + 3} pages written to {out}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(Path(sys.argv[1]))
