r"""Builds the translations into mod_override\Data\: a copy per language of every Data file that has
one, with our mods' texts in it.

    python build_data.py "<game folder>" mod_override       (from translations\, after every other data mod)

The game opens <file>.xml.<id> for the chosen language when it exists (camera-smooth\ENGINE_NOTES.md,
"Languages"; the ids are langs.py's), else <file>.xml, which is English. For each language and file:

- a file one of our mods changed (langs.mod_file: help.xml, typenames.xml, options.xml): our mods'
  copy, every text the mods did not change taken from the language's own copy (the game's, or the
  translated English for the new languages), every text they changed or added from
  text\<code>.json, so the language keeps the mods' rules, names and icons;
- a file no mod changed: for the game's languages nothing (the game's own file is right); for the
  new languages the English file with every translated text, when text\<code>.json has any.

And Data\revived_strings.xml.<id>, the DLL's own texts (dll_strings.txt) for camera-smooth\src\lang.c,
and launcher_res\locale\<code>.ini, the launcher's texts (launcher.json) for
camera-smooth\src\launcher_lang.c, in UTF-16 so GetPrivateProfileStringW reads them as they are.
A text with no translation stays English; the build lists how many.
"""
import json
import os
import re
import sys
from xml.sax.saxutils import escape, quoteattr

import langs as L
import xmltext as X


def load_store(code):
    p = os.path.join(L.TEXT, code + ".json")
    if not os.path.exists(p):
        return {}
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def translate(xml, store):
    """the unit's text in the store's language (outer white space kept), or None"""
    lead, core, trail = X.split_space(xml)
    if not X.translatable(core):
        return xml
    t = store.get(core)
    return None if t is None else lead + t + trail


IDENT_RE = re.compile(r"^(?=\S*[_\\/.])\S+$|^[A-Z0-9_]{3,}$")  # SYSTEM_TOGGLE, m_steppe, .\Saves, file.xml


def kept_english(name, game):
    """paths of texts that look like internal names and that the game's four translations all
    leave in English: the new languages keep them English too (the code may look them up)"""
    en = X.units(X.load(os.path.join(game, "Data", name)))
    langs = [X.units(X.load(p)) for p in (os.path.join(game, "Data", name + s) for s, _ in L.GAME_LANGS.values())
             if os.path.exists(p)]
    out = set()
    for path, e in en.items():
        core = X.inner(e).strip()
        if IDENT_RE.match(core) and langs and all(path in u and X.inner(u[path]).strip() == core for u in langs):
            out.add(path)
    return out


_kept = {}


def build_file(name, code, store, game, out):
    suffix = L.ALL_LANGS[code][0]
    mod_path = L.mod_file(name)
    new_lang = code in L.NEW_LANGS
    if not mod_path and not new_lang:
        return None
    en_path = os.path.join(game, "Data", name)
    if not os.path.exists(en_path):
        return None
    en = X.units(X.load(en_path))
    own_path = os.path.join(game, "Data", name + suffix)
    own = X.units(X.load(own_path)) if not new_lang and os.path.exists(own_path) else {}
    tree = X.load(mod_path or en_path)
    if new_lang and name not in _kept:
        _kept[name] = kept_english(name, game)
    done = english = 0
    for path, e in X.units(tree).items():
        xml = X.inner(e)
        if not X.translatable(xml) or (new_lang and path in _kept[name]):
            continue
        if path in en and X.same_words(X.inner(en[path]), xml):  # the game's own text
            text = X.inner(own[path]) if path in own else translate(X.inner(en[path]), store)
        else:  # a text our mods changed or added; untranslated, the game's text it replaced beats English
            text = translate(xml, store)
            if text is None and path in own:
                text = X.inner(own[path])
        if text is None:
            english += 1
            continue
        X.set_inner(e, text)
        done += 1
    if not mod_path and not done:
        return None  # nothing translated yet: the game's English file serves
    X.save(tree, os.path.join(out, "Data", name + suffix))
    return done, english


def build_dll(code, store, out):
    suffix = L.ALL_LANGS[code][0]
    lines, missing = [], 0
    with open(L.DLL_STRINGS, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            lead, core, trail = X.split_space(line)
            t = store.get("DLL:" + core)  # merge.key_of
            if t is None:
                missing += 1
                continue
            lines.append(f"\t<STRING en={quoteattr(line)}>{escape(lead + t + trail)}</STRING>")
    with open(os.path.join(out, "Data", "revived_strings.xml" + suffix), "w", encoding="utf-8", newline="\r\n") as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n<!-- the DLL\'s own texts (translations\\dll_strings.txt), '
                'made by translations\\tools\\build_data.py -->\n<ROOT>\n' + "\n".join(lines) + "\n</ROOT>\n")
    return len(lines), missing


def build_launcher(out):
    r"""launcher_res\locale\<code>.ini for each language of launcher.json."""
    folder = os.path.join(out, "launcher_res", "locale")
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(L.MOD, "launcher.json"), encoding="utf-8") as f:
        texts = {c: t for c, t in json.load(f).items() if c != "_"}
    for code, t in texts.items():
        lines = ["[launcher]"] + [f"{k}={v}" for k, v in sorted(t.items())]
        with open(os.path.join(folder, code + ".ini"), "w", encoding="utf-16", newline="\r\n") as f:
            f.write("\n".join(lines) + "\n")
    return len(texts)


def main(game, out):
    os.makedirs(os.path.join(out, "Data"), exist_ok=True)
    for f in os.listdir(os.path.join(out, "Data")):  # the last build's files
        os.remove(os.path.join(out, "Data", f))
    for code, (suffix, lang) in L.ALL_LANGS.items():
        store = load_store(code)
        parts = []
        for name in L.LOCALIZED:
            r = build_file(name, code, store, game, out)
            if r:
                parts.append(f"{name}{suffix} {r[0]}" + (f" ({r[1]} English)" if r[1] else ""))
        n, missing = build_dll(code, store, out)
        parts.append(f"DLL {n}" + (f" ({missing} English)" if missing else ""))
        print(f"{lang}: " + ", ".join(parts))
    print(f"launcher: {build_launcher(out)} languages")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
