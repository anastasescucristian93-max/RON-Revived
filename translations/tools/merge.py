r"""Checks the translators' answers and adds them to text\<code>.json.

    python merge.py check <code> <chunk>   checks work\<code>\<chunk>.json against work\chunks\<chunk>.json
                                           (exit code 1 when something must be fixed)
    python merge.py <code>                 adds every answer of work\<code>\ that passes the check

An answer file is a JSON object {id: translation}. A translation must keep the English text's
tokens (inline tags, $NUMBER0-style values, #ICON27, printf formats, the {highlight} braces, [ ] and
| marks), be well-formed XML content (& as &amp;, < as &lt;) and not be empty.
"""
import json
import os
import sys

from lxml import etree

import langs as L
import xmltext as X


def key_of(item):
    r"""the text's key in text\<code>.json: the English, with "DLL:" before the DLL's own texts (the
    rank "Private" is not the Workshop's "Private")"""
    return "DLL:" + item["en"] if item.get("file") == "DLL" else item["en"]


def read_json(p):
    with open(p, encoding="utf-8-sig") as f:
        return json.load(f)


def problems(item, text):
    out = []
    if not isinstance(text, str) or not text.strip():
        return ["empty"]
    if X.tokens(item["en"]) != X.tokens(text):
        want, got = X.tokens(item["en"]), X.tokens(text)
        lost = [t for t in set(want) if want.count(t) > got.count(t)]
        extra = [t for t in set(got) if got.count(t) > want.count(t)]
        out.append(f"tokens differ (missing {lost}, extra {extra})")
    try:
        etree.fromstring(f"<x>{text}</x>".encode("utf-8"), X.PARSER)
    except etree.XMLSyntaxError as e:
        out.append(f"not well-formed XML ({e}); write & as &amp;, < as &lt;")
    return out


def check(code, chunk):
    items = read_json(os.path.join(L.WORK, "chunks", chunk + ".json"))
    ans_path = os.path.join(L.WORK, code, chunk + ".json")
    if not os.path.exists(ans_path):
        print(f"no answer file {ans_path}")
        return 1
    try:
        ans = read_json(ans_path)
    except json.JSONDecodeError as e:
        print(f"{ans_path} is not valid JSON: {e}")
        return 1
    ids = {it["id"]: it for it in items}
    bad = 0
    for it in items:
        if it["id"] not in ans:
            print(f"{it['id']}: missing")
            bad += 1
            continue
        for p in problems(it, ans[it["id"]]):
            print(f"{it['id']}: {p}")
            bad += 1
    for k in ans:
        if k not in ids:
            print(f"{k}: not an id of this chunk")
            bad += 1
    print(f"{chunk} ({code}): {len(items)} texts, {bad} problem(s)")
    return 1 if bad else 0


def merge(code):
    store_path = os.path.join(L.TEXT, code + ".json")
    store = read_json(store_path) if os.path.exists(store_path) else {}
    folder = os.path.join(L.WORK, code)
    added = bad = 0
    for f in sorted(os.listdir(folder)) if os.path.isdir(folder) else []:
        chunk = os.path.join(L.WORK, "chunks", f)
        if not f.endswith(".json") or not os.path.exists(chunk):
            continue
        ans = read_json(os.path.join(folder, f))
        for it in read_json(chunk):
            text = ans.get(it["id"])
            if text is None or problems(it, text):
                bad += 1
                continue
            if store.get(key_of(it)) != text:
                store[key_of(it)] = text
                added += 1
    os.makedirs(L.TEXT, exist_ok=True)
    with open(store_path, "w", encoding="utf-8") as f:
        json.dump(dict(sorted(store.items())), f, ensure_ascii=False, indent=0)
    print(f"{code}: {added} translations added or changed, {bad} left out (run check), {len(store)} in all")


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "check":
        sys.exit(check(sys.argv[2], sys.argv[3]))
    if len(sys.argv) == 2 and sys.argv[1] in L.ALL_LANGS:
        merge(sys.argv[1])
    else:
        sys.exit(__doc__)
