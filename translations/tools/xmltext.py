"""The translatable texts of the game's XML files, as units with a stable path.

A unit is the topmost element that holds text of its own (its text, or the tail of a child), so a
help entry with <BULLET/> marks inside is one unit and its content is the inner XML. The path names
every element by tag and, where it has one, a key attribute (hash, name, enum, ...), else by its
position among siblings of the same tag; the same path finds the same text in every language's copy
of a file, and in our mods' changed copy.
"""
import re

from lxml import etree

KEY_ATTRS = ("hash", "name", "enum", "tribe", "age", "mood", "id", "type")
SKIP_TAGS = {"ENGLISH_NAME", "MAXPROFILELEN"}  # internal names and numbers, never shown translated
PARSER = etree.XMLParser(remove_blank_text=False, remove_comments=False, strip_cdata=False, resolve_entities=False)

# what must survive a translation unchanged: inline tags and comments, $NUMBER0 / $TRIBENAME0,
# #ICON27, printf formats, \n, and the {highlight} / [ ] / | marks (their count)
TOKEN_RE = re.compile(r"<[^>]+>|\$[A-Z_]+\d*|#ICON\d+|%%|%[-+0-9.]*[dsifxXc]|\\n|[{}\[\]|]")


def load(path):
    with open(path, "rb") as f:
        data = f.read()
    if data.startswith(b"\xef\xbb\xbf"):
        data = data[3:]
    return etree.fromstring(data, PARSER).getroottree()


def save(tree, path):
    data = etree.tostring(tree, xml_declaration=True, encoding="UTF-8")
    with open(path, "wb") as f:
        f.write(data)


def _own_text(e):
    if (e.text or "").strip():
        return True
    return any((c.tail or "").strip() for c in e)


def _step(e):
    for a in KEY_ATTRS:
        if a in e.attrib:
            return f"{e.tag}[{a}={e.attrib[a]}]"
    parent = e.getparent()
    if parent is None:
        return e.tag
    same = [c for c in parent if c.tag == e.tag]
    return f"{e.tag}#{same.index(e)}"


def path_of(e):
    steps = []
    while e is not None:
        steps.append(_step(e))
        e = e.getparent()
    return "/".join(reversed(steps))


def inner(e):
    """the element's content as XML text (escaped text, child tags, tails)"""
    out = [_escape(e.text or "")]
    for c in e:
        out.append(etree.tostring(c, encoding="unicode", with_tail=False))
        out.append(_escape(c.tail or ""))
    return "".join(out)


def _escape(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def set_inner(e, xml):
    """replaces the element's content by the XML text (raises if it is not well formed)"""
    frag = etree.fromstring(f"<x>{xml}</x>".encode("utf-8"), PARSER)
    for c in list(e):
        e.remove(c)
    e.text = frag.text
    for c in list(frag):
        e.append(c)


def units(tree):
    """path -> element, in document order"""
    out = {}

    def walk(e):
        for c in e:
            if not isinstance(c.tag, str):
                continue
            if c.tag in SKIP_TAGS:
                continue
            if _own_text(c):
                p, n = path_of(c), 1
                while p in out:  # the same key twice under one parent
                    p, n = f"{path_of(c)}~{n}", n + 1
                out[p] = c
            else:
                walk(c)

    walk(tree.getroot())
    return out


def tokens(xml):
    """the tokens a translation must keep, as a sorted list"""
    return sorted(TOKEN_RE.findall(xml))


def translatable(xml):
    """False for units with no words (a stray "--" between tags, numbers)"""
    return re.search(r"[^\W\d_]", plain(re.sub(r"<!--.*?-->", "", xml, flags=re.S))) is not None


def same_words(a, b):
    """equal but for white space (a mod may drop double spaces after full stops)"""
    return a.split() == b.split()


def split_space(s):
    """(leading whitespace, the text, trailing whitespace): translators get the middle only"""
    core = s.strip()
    if not core:
        return s, "", ""
    i = s.index(core)
    return s[:i], core, s[i + len(core):]


def plain(xml):
    """the visible text of a unit (tags dropped, entities read), for measuring"""
    s = re.sub(r"<BULLET\s*/>", "\n", xml)
    s = re.sub(r"<[^>]+>", "", s)
    return s.replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", '"').replace("&amp;", "&")
