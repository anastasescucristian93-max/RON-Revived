r"""Makes the chunks the translators work on: work\chunks\<set>_NN.json.

    python extract.py game     the interface files (langs.INTERFACE); game_00 is typenames.xml alone,
                               the names every other chunk uses (the glossary)
    python extract.py help     help.xml
    python extract.py mods     every text our mods add or change (their copies of the Data files
                               against the game's) and the DLL's texts (dll_strings.txt)

A chunk is a list of {"id", "en", "file", "ref"}: "en" the English text without its outer white
space (inline tags as XML), "ref" the game's own German / Spanish / French / Italian for short
texts (for mods: the game's translation of the text the mod replaced). Each English text is in
one chunk only, the first time it appears. Texts already in every language's text\<code>.json
are left out, so after a change only the new texts come back.
"""
import json
import os
import sys

import langs as L
import xmltext as X

WORDS_PER_CHUNK = 6000
SHORT = 12  # texts of up to this many words carry the game's translations as reference


def done_everywhere(en, codes):
    for c in codes:
        p = os.path.join(L.TEXT, c + ".json")
        if not os.path.exists(p):
            return False
        with open(p, encoding="utf-8") as f:
            if en not in json.load(f):
                return False
    return True


def game_units(name, suffix=""):
    p = L.game_file(name, suffix)
    return X.units(X.load(p)) if os.path.exists(p) else {}


def collect(files, codes):
    items, seen = [], set()
    for name in files:
        en_units = game_units(name)
        refs = {c: game_units(name, L.GAME_LANGS[c][0]) for c in L.GAME_LANGS}
        for path, e in en_units.items():
            xml = X.inner(e)
            _, core, _ = X.split_space(xml)
            if not X.translatable(core) or core in seen:
                continue
            seen.add(core)
            if done_everywhere(core, codes):
                continue
            item = {"en": core, "file": name}
            if len(X.plain(core).split()) <= SHORT:
                item["ref"] = {c: X.split_space(X.inner(u[path]))[1] for c, u in refs.items() if path in u}
            items.append(item)
    return items


def mod_items(codes):
    items, seen = [], set()
    for name in L.LOCALIZED:
        mp = L.mod_file(name)
        if not mp:
            continue
        en_units = game_units(name)
        refs = {c: game_units(name, L.GAME_LANGS[c][0]) for c in L.GAME_LANGS}
        for path, e in X.units(X.load(mp)).items():
            xml = X.inner(e)
            if path in en_units and X.same_words(X.inner(en_units[path]), xml):
                continue  # the game's own text, translated by the game (or by the game chunks)
            _, core, _ = X.split_space(xml)
            if not X.translatable(core) or core in seen or done_everywhere(core, codes):
                continue
            seen.add(core)
            item = {"en": core, "file": name}
            if path in en_units:
                item["was"] = X.split_space(X.inner(en_units[path]))[1]
                item["ref"] = {c: X.split_space(X.inner(u[path]))[1] for c, u in refs.items() if path in u}
            items.append(item)
    with open(L.DLL_STRINGS, encoding="utf-8") as f:
        for line in f:
            core = X.split_space(line.rstrip("\n").replace("\\n", "\n"))[1]
            if core and not line.startswith("#") and core not in seen and not done_everywhere("DLL:" + core, codes):
                seen.add(core)
                items.append({"en": core, "file": "DLL"})
    return items


def chunks(items, first_alone=None):
    out, cur, words = [], [], 0
    for it in items:
        if first_alone and cur and cur[-1]["file"] == first_alone and it["file"] != first_alone:
            out.append(cur)
            cur, words = [], 0
        cur.append(it)
        words += len(X.plain(it["en"]).split())
        if words >= WORDS_PER_CHUNK and it["file"] != first_alone:
            out.append(cur)
            cur, words = [], 0
    if cur:
        if out and out[-1][-1]["file"] != first_alone and words < WORDS_PER_CHUNK // 4:
            out[-1].extend(cur)  # a small rest goes with the chunk before
        else:
            out.append(cur)
    return out


def glossaries():
    r"""work\glossary\<code>.json: the English names of typenames.xml and each language's (the game's
    own for its languages, text\<code>.json's for ours), for the translators to keep names consistent"""
    out = os.path.join(L.WORK, "glossary")
    os.makedirs(out, exist_ok=True)
    en_units = game_units("typenames.xml")
    for code in L.ALL_LANGS:
        if code in L.GAME_LANGS:
            lu = game_units("typenames.xml", L.GAME_LANGS[code][0])
            pairs = {X.inner(e).strip(): X.inner(lu[p]).strip() for p, e in en_units.items() if p in lu}
        else:
            p = os.path.join(L.TEXT, code + ".json")
            if not os.path.exists(p):
                continue
            with open(p, encoding="utf-8") as f:
                store = json.load(f)
            pairs = {X.inner(e).strip(): store[X.inner(e).strip()] for e in en_units.values()
                     if X.inner(e).strip() in store}
        with open(os.path.join(out, code + ".json"), "w", encoding="utf-8") as f:
            json.dump(pairs, f, ensure_ascii=False, indent=0)
        print(f"glossary {code}: {len(pairs)} names")


def main():
    what = sys.argv[1] if len(sys.argv) > 1 else ""
    if what == "glossary":
        return glossaries()
    if what == "game":
        parts = chunks(collect(L.INTERFACE, L.NEW_LANGS), first_alone="typenames.xml")
    elif what == "help":
        parts = chunks(collect(L.LATER, L.NEW_LANGS))
    elif what == "mods":
        parts = chunks(mod_items(L.ALL_LANGS))
    else:
        sys.exit(__doc__)
    out = os.path.join(L.WORK, "chunks")
    os.makedirs(out, exist_ok=True)
    for f in os.listdir(out):
        if f.startswith(what + "_"):
            os.remove(os.path.join(out, f))
    n = 0
    for i, part in enumerate(parts):
        for j, it in enumerate(part):
            it["id"] = f"{what[0]}{i:02d}{j:04d}"
        with open(os.path.join(out, f"{what}_{i:02d}.json"), "w", encoding="utf-8") as f:
            json.dump(part, f, ensure_ascii=False, indent=1)
        words = sum(len(X.plain(it["en"]).split()) for it in part)
        n += len(part)
        print(f"{what}_{i:02d}.json: {len(part)} texts, {words} words")
    print(f"{n} texts in {len(parts)} chunks")


if __name__ == "__main__":
    main()
