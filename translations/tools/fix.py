r"""Corrects translations after review: in text\<code>.json and in the answer files of work\<code>\,
so a later merge does not bring the old wording back.

    python fix.py <code> <English regex> <old> <new>     replace old by new in the translations of every
                                                         English text the regex matches
    python fix.py <code> --set <English text> <new>      the whole translation of one English text
    python fix.py <code> --file <json>                   many at once: a JSON object {English: new translation}

The DLL's own texts are "DLL:<English>" (merge.key_of). Each changed translation is checked like a
translator's answer (merge.problems) and printed.
"""
import json
import os
import re
import sys

import langs as L
import merge


def main(argv):
    if len(argv) == 4 and argv[2] == "--file":
        wanted = merge.read_json(argv[3])
        match = lambda en: en in wanted  # noqa: E731
        change = lambda en, t: wanted[en]  # noqa: E731
    elif len(argv) != 5:
        sys.exit(__doc__)
    code = argv[1]
    if len(argv) == 4:
        pass
    elif argv[2] == "--set":
        match = lambda en: en == argv[3]  # noqa: E731
        change = lambda en, t: argv[4]  # noqa: E731
    else:
        rx = re.compile(argv[2])
        match = lambda en: rx.search(en) is not None  # noqa: E731
        change = lambda en, t: t.replace(argv[3], argv[4])  # noqa: E731
    store_path = os.path.join(L.TEXT, code + ".json")
    store = merge.read_json(store_path)
    new = {}
    for en, t in store.items():
        if match(en):
            nt = change(en, t)
            if nt != t:
                bad = merge.problems({"en": en[4:] if en.startswith("DLL:") else en}, nt)
                if bad:
                    print(f"left out, {bad}: {en!r}")
                    continue
                new[en] = nt
                print(f"{en[:60]!r}: {nt[:80]!r}")
    store.update(new)
    with open(store_path, "w", encoding="utf-8") as f:
        json.dump(dict(sorted(store.items())), f, ensure_ascii=False, indent=0)
    folder = os.path.join(L.WORK, code)
    for fn in os.listdir(folder) if os.path.isdir(folder) else []:
        chunk = os.path.join(L.WORK, "chunks", fn)
        if not os.path.exists(chunk):
            continue
        ans_path = os.path.join(folder, fn)
        ans = merge.read_json(ans_path)
        hit = False
        for it in merge.read_json(chunk):
            if merge.key_of(it) in new and it["id"] in ans:
                ans[it["id"]] = new[merge.key_of(it)]
                hit = True
        if hit:
            with open(ans_path, "w", encoding="utf-8") as f:
                json.dump(ans, f, ensure_ascii=False, indent=1)
    print(f"{code}: {len(new)} translation(s) changed")


if __name__ == "__main__":
    main(sys.argv)
