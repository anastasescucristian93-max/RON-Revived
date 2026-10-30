"""Genereaza src/winmm_proxy.inc si src/winmm.def din winmm.dll (32-bit) al sistemului.

Fiecare export devine un stub `jmp [g_winmm_procs + i*4]`; pointerii se rezolva in
DllMain din winmm.dll-ul original. Rulat o singura data; fisierele generate sunt in repo.

    python -I tools/gen_winmm_proxy.py   (necesita pefile)
"""
import os
import pefile

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")
pe = pefile.PE(r"C:\Windows\SysWOW64\winmm.dll")
exports = [(e.ordinal, e.name.decode() if e.name else None) for e in pe.DIRECTORY_ENTRY_EXPORT.symbols]

inc = [
    "/* GENERAT de tools/gen_winmm_proxy.py - nu edita manual. */",
    f"#define WINMM_PROC_COUNT {len(exports)}",
    "static const struct { const char *name; WORD ordinal; } g_winmm_exports[] = {",
]
for ordinal, name in exports:
    inc.append(f'    {{ {("\"" + name + "\"") if name else "NULL"}, {ordinal} }},')
inc.append("};")
inc.append("void *g_winmm_procs[WINMM_PROC_COUNT];")
inc.append("__asm__(\".intel_syntax noprefix\\n\"")
for i, (ordinal, name) in enumerate(exports):
    stub = f"_wmstub_{name or 'ord%d' % ordinal}"
    inc.append(f'    ".globl {stub}\\n{stub}:\\n    jmp dword ptr [_g_winmm_procs + {i * 4}]\\n"')
inc.append("    \".att_syntax prefix\\n\");")

deff = ["LIBRARY winmm", "EXPORTS"]
for ordinal, name in exports:
    if name:
        deff.append(f"    {name}=wmstub_{name} @{ordinal}")
    else:
        deff.append(f"    ord{ordinal}=wmstub_ord{ordinal} @{ordinal} NONAME")

with open(os.path.join(SRC, "winmm_proxy.inc"), "w", newline="\n") as f:
    f.write("\n".join(inc) + "\n")
with open(os.path.join(SRC, "winmm.def"), "w", newline="\n") as f:
    f.write("\n".join(deff) + "\n")
print(f"{len(exports)} exporturi generate")
