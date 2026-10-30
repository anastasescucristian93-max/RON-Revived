r"""Names the game's internal strings: python istr.py <offset> [more offsets]

Code reads them as [int_str_array] + 0x10, then + offset; the strings are 0x14 bytes each, in the
order of Data\internal_strings.xml, so the entry is offset / 0x14 (checked on the Tribes::init
strings: 0x49D4 TRIBES, 0x49E8 TRIBE, 0xE7F4 FILE, 0x16738 KEY, 0x21E08 TRIBES\).
"""
import os
import re
import sys

GAME = os.environ.get("RON_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations")
rows = re.findall(r"<STRING[^>]*?(?:/>|>(.*?)</STRING>)",
                  open(os.path.join(GAME, "Data", "internal_strings.xml"), encoding="utf-8").read(), re.S)
for arg in sys.argv[1:]:
    i = int(arg, 16) // 0x14
    print("%s  %r" % (arg, rows[i] if 0 <= i < len(rows) else None))
