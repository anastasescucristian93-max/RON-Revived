r"""Reverse-engineering helper for riseofnations.exe (Steam EE, timestamp 6674863F), with the names
from the game's own sbl\rise_z.map.

    python rdis.py find <text> [more text...]     symbols whose readable name contains every text
    python rdis.py dis <address|symbol> [bytes]   disassemble a function (calls and data named)
    python rdis.py xref <address|symbol>          instructions that call / jump to / mention it
    python rdis.py name <address>                 the symbol an address belongs to

The game folder is taken from RON_DIR or the default Steam path. Names are undecorated with
DbgHelp's UnDecorateSymbolName. The symbol table is cached next to this script (rdis_syms.txt);
xref scans the whole code section once and caches it too (rdis_text.txt).
"""
import bisect
import ctypes
import os
import re
import sys

import capstone
import pefile

GAME = os.environ.get("RON_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations")
HERE = os.path.dirname(os.path.abspath(__file__))
SYMS = os.path.join(HERE, "rdis_syms.txt")
TEXT = os.path.join(HERE, "rdis_text.txt")


def undecorate(name):
    try:
        dbghelp = ctypes.windll.dbghelp
    except AttributeError:
        return name
    buf = ctypes.create_string_buffer(1024)
    n = dbghelp.UnDecorateSymbolName(name.encode(), buf, 1024, 0x1000)  # UNDNAME_NAME_ONLY off: full
    n = dbghelp.UnDecorateSymbolName(name.encode(), buf, 1024, 0)
    return buf.value.decode(errors="replace") if n else name


def load_syms():
    if os.path.exists(SYMS):
        rows = [line.rstrip("\n").split("\t") for line in open(SYMS, encoding="utf-8")]
        return [(int(a, 16), n) for a, n in rows]
    out = []
    pat = re.compile(r"^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s", re.I)
    for line in open(os.path.join(GAME, "sbl", "rise_z.map"), encoding="latin-1"):
        m = pat.match(line)
        if m and int(m.group(2), 16) >= 0x401000:
            out.append((int(m.group(2), 16), undecorate(m.group(1))))
    out.sort()
    with open(SYMS, "w", encoding="utf-8") as fh:
        fh.writelines("%08X\t%s\n" % (a, n) for a, n in out)
    return out


SYM = load_syms()
ADDRS = [a for a, _ in SYM]
BY_ADDR = dict(SYM)


def name_of(a):
    i = bisect.bisect_right(ADDRS, a) - 1
    if i < 0:
        return None
    base, n = SYM[i]
    return n if base == a else "%s+%X" % (n, a - base)


def resolve(arg):
    try:
        return int(arg, 16)
    except ValueError:
        hits = [(a, n) for a, n in SYM if arg in n]
        exact = [h for h in hits if re.search(r"(^|::|\s)%s\(" % re.escape(arg.split("::")[-1]), h[1])]
        hits = exact or hits
        if not hits:
            sys.exit("no symbol matches " + arg)
        if len(hits) > 1:
            for a, n in hits[:20]:
                print("%08X  %s" % (a, n))
            sys.exit("%d matches, give the address" % len(hits))
        return hits[0][0]


def image():
    pe = pefile.PE(os.path.join(GAME, "riseofnations.exe"), fast_load=True)
    return pe, pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase


def annotate(op):
    def sub(m):
        v = int(m.group(0), 16)
        if v >= 0x401000:
            n = name_of(v)
            if n and "+" not in n:
                return "%s <%s>" % (m.group(0), n)
        return m.group(0)
    return re.sub(r"0x[0-9a-f]{6,8}", sub, op)


def dis(a, size=None):
    pe, img, base = image()
    i = bisect.bisect_right(ADDRS, a)
    end = ADDRS[i] if i < len(ADDRS) else a + 0x400
    if size:
        end = a + size
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    print("%08X  %s" % (a, name_of(a)))
    for ins in md.disasm(img[a - base:end - base], a):
        print("  %08X  %-6s %s" % (ins.address, ins.mnemonic, annotate(ins.op_str)))


def text_listing():
    if os.path.exists(TEXT):
        return open(TEXT, encoding="utf-8").read().splitlines()
    pe, img, base = image()
    t = [s for s in pe.sections if s.Name.startswith(b".text")][0]
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.skipdata = True
    lines = ["%08X %s %s" % (a, mn, op) for a, _, mn, op in
             md.disasm_lite(img[t.VirtualAddress:t.VirtualAddress + t.Misc_VirtualSize], base + t.VirtualAddress)]
    with open(TEXT, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines))
    return lines


def xref(a):
    needle = "0x%x" % a
    for line in text_listing():
        if needle in line and re.search(r"\b%s\b" % needle, line):
            addr = int(line[:8], 16)
            print("%s   [%s]" % (line, name_of(addr)))


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    cmd, args = sys.argv[1], sys.argv[2:]
    if cmd == "find":
        for a, n in SYM:
            if n.startswith("__") or "`" in n or n.startswith("void __cdecl `"):
                continue  # unwind funclets, EH tables, static locals
            if all(x.lower() in n.lower() for x in args):
                print("%08X  %s" % (a, n))
    elif cmd == "dis":
        dis(resolve(args[0]), int(args[1], 16) if len(args) > 1 else None)
    elif cmd == "xref":
        xref(resolve(args[0]))
    elif cmd == "name":
        print(name_of(int(args[0], 16)))
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
