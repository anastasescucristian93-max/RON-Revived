r"""Compares the game's desync logs from two machines and shows where they first differ.

    python desync_compare.py <log A> <log B> [--context N]

The logs are the SyncLogger's "SyncLog TurnLog <player id>.txt" files (also SendLog / ReceiveLog /
Standard), written to %APPDATA%\Microsoft Games\Rise of Nations\Logs\ when a multiplayer game goes
out of sync, with tracking turned on in synclogger.ini (see camera-smooth/ENGINE_NOTES.md, "Desync
(out-of-sync) logging"). Each turn starts with "**** BEGIN TURN 000123 ****"; with ChecksumSync=1
every turn lists one checksum per part of the game state ("  units: %u", "  guys: %u", ...).

The script splits both files into turns, finds the first turn both files hold whose lines differ,
names the checksum parts that differ in it, and prints the differing lines of that turn.
"""
import argparse
import difflib
import re
import sys

TURN = re.compile(r"\*{4} BEGIN TURN (\d+) \*{4}")
PART = re.compile(r"^\s+([a-z_]+): (\d+)\s*$")


def read_turns(path):
    head, turns, cur = [], {}, None
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\r\n").replace("\0", "")
            m = TURN.search(line)
            if m:
                cur = int(m.group(1))
                turns.setdefault(cur, [])
                continue
            (head if cur is None else turns[cur]).append(line)
    return head, turns


def parts(lines):
    out = {}
    for line in lines:
        m = PART.match(line)
        if m:
            out.setdefault(m.group(1), []).append(m.group(2))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--context", type=int, default=2, help="lines of context around each change")
    args = ap.parse_args()
    head_a, ta = read_turns(args.a)
    head_b, tb = read_turns(args.b)
    for label, head in (("A", head_a), ("B", head_b)):
        first = next((l for l in head if l.strip()), "")
        print(f"{label}: {len(ta if label == 'A' else tb)} turns; {first}")
    common = sorted(set(ta) & set(tb))
    if not common:
        print("No turn appears in both files (no tracking data, or the histories do not overlap).")
        sys.exit(1)
    print(f"Turns in both: {common[0]}..{common[-1]}")
    for t in common:
        if ta[t] == tb[t]:
            continue
        print(f"\nFirst differing turn: {t}")
        pa, pb = parts(ta[t]), parts(tb[t])
        diff_parts = [k for k in dict.fromkeys(list(pa) + list(pb)) if pa.get(k) != pb.get(k)]
        if diff_parts:
            print("Checksum parts that differ: " + ", ".join(diff_parts))
        for line in difflib.unified_diff(ta[t], tb[t], "A", "B", n=args.context, lineterm=""):
            print(line)
        return
    print("All turns in both files match line for line.")


if __name__ == "__main__":
    main()
