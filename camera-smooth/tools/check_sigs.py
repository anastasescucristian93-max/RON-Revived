"""Checks every hook signature in src/ron_camera.c against the installed riseofnations.exe.

    python -I tools/check_sigs.py [path\to\riseofnations.exe]   (needs pefile)
"""
import os, re, sys
import pefile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
EXE = sys.argv[1] if len(sys.argv) > 1 else r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations\riseofnations.exe"
pe = pefile.PE(EXE, fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
src = open(os.path.join(ROOT, "src", "ron_camera.c"), encoding="utf-8").read()
macros = {m.group(1): int(m.group(2), 16) for m in re.finditer(r"#define (VA_\w+)\s+(0x[0-9A-Fa-f]+)u?", src)}

ok = True
for m in re.finditer(r'\{ "(\w+)", (\w+), \{([^}]*)\}, (\d+), (\d+)', src):
    name, va_s, sig, n = m.group(1), m.group(2), m.group(3), int(m.group(4))
    va = macros[va_s] if va_s in macros else int(va_s.rstrip("uU"), 16)
    want = bytes(int(b, 16) for b in re.findall(r"0x([0-9A-Fa-f]{2})", sig))[:n]
    have = pe.get_data(va - base, n)
    good = want == have
    ok &= good
    print(f"{'OK ' if good else 'BAD'} {name:22} {va:08x}  {have.hex(' ')}" + ("" if good else f"\n    expected {want.hex(' ')}"))
sys.exit(0 if ok else 1)
