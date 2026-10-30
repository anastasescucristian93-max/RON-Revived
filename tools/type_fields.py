r"""Lists which XML tag each field of a type is read from, in the order UnitType::init (or another
init) reads them: python type_fields.py [init address, default UnitType::init 0x0061AB50]

The tag names live in the game's string table (int_str_array + 0x10, 0x14 bytes per entry), so
the offset is printed together with the field it is stored to; match them with the order of the
tags in unitrules.xml.
"""
import re
import subprocess
import sys

addr = sys.argv[1] if len(sys.argv) > 1 else "0061AB50"
out = subprocess.run([sys.executable, "-I", "rdis.py", "dis", addr], capture_output=True, text=True).stdout.splitlines()
last = None
for i, line in enumerate(out):
    m = re.search(r"add\s+eax, (0x[0-9a-f]+)$", line)
    if m:
        last = m.group(1)
    m = re.search(r"call\s+0x[0-9a-f]+ <[^>]*?(\w+::\w+)\(", line)
    if m and "XMLElement" in line:
        store = ""
        for j in range(i + 1, min(i + 8, len(out))):
            s = re.search(r"mov\s+(?:dword|word|byte) ptr \[esi \+ (0x[0-9a-f]+)\]", out[j])
            if s:
                store = s.group(1)
                break
        print("%-28s string %-8s -> +%s" % (m.group(1), last, store))
