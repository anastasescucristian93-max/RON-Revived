"""Run riseofnations.exe's Balance::type_damage(attacker, target) under unicorn for chosen pairs.
Fake ObjectType objects carry only what the function reads: index (+4), OBJ_MASK (+0x1e4),
cached age (+0x278), field +0x218, FLAGS (+0x2b4). Virtual is(N,0) is answered in Python."""
import re, struct, sys
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *

GAME = r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations"
TYPE_DAMAGE = 0x57FB50

u = open(GAME + r"\Data\unitrules.xml", encoding="latin-1").read()
b = open(GAME + r"\Data\buildingrules.xml", encoding="latin-1").read()
def tag(s, t):
    m = re.search(f"<{t}>(.*?)</{t}>", s, re.S); return m.group(1).strip() if m else ""
units = re.findall(r"<UNIT>(.*?)</UNIT>", u, re.S)
blds = re.findall(r"<BUILDING>(.*?)</BUILDING>", b, re.S)
types = {}  # index -> dict
for i, s in enumerate(units):
    types[0x32 + i] = dict(name=tag(s, "NAME"), mask=tag(s, "OBJ_MASK"), flags=tag(s, "FLAGS"),
                           frm=tag(s, "FROM"), atk=tag(s, "ATTACK"), rec=tag(s, "RECHARGE"))
for i, s in enumerate(blds):
    types[0x19E + i] = dict(name=tag(s, "NAME"), mask=tag(s, "OBJ_MASKS"), flags="", frm="none",
                            hp=tag(s, "HITS"), armor=tag(s, "ARMOR"))
byname = {}
for k, t in types.items(): byname.setdefault(t["name"], k)

def bits(s, upper):
    v = 0
    for c in s:
        if upper and "A" <= c <= "Z": v |= 1 << (ord(c) - 65)
        elif not upper and "a" <= c <= "z": v |= 1 << (ord(c) - 97)
        elif c in "123456": v |= 1 << (26 + int(c) - 1)
    return v

def lineage(k):
    """Indices in k's upgrade line (ancestors and descendants)."""
    out = {k}; n = types[k]["name"]
    while types[byname.get(n, k)]["frm"] not in ("none", "") and types[byname.get(n, k)]["frm"] in byname:
        n = types[byname[n]]["frm"]; out.add(byname[n])
    changed = True
    while changed:
        changed = False
        for j, t in types.items():
            if j not in out and t["frm"] in byname and byname[t["frm"]] in out:
                out.add(j); changed = True
    return out

pe = pefile.PE(GAME + r"\riseofnations.exe")
base = pe.OPTIONAL_HEADER.ImageBase
mu = Uc(UC_ARCH_X86, UC_MODE_32)
size = (pe.OPTIONAL_HEADER.SizeOfImage + 0xFFF) & ~0xFFF
mu.mem_map(base, size)
for s in pe.sections:
    mu.mem_write(base + s.VirtualAddress, s.get_data())

HEAP, STACK, STUB = 0x10000000, 0x20000000, 0x30000000
mu.mem_map(HEAP, 0x1000000); mu.mem_map(STACK, 0x100000); mu.mem_map(STUB, 0x1000)
IS_STUB, RET0_STUB, END = STUB, STUB + 0x10, STUB + 0x20
mu.mem_write(IS_STUB, b"\xC2\x08\x00"); mu.mem_write(RET0_STUB, b"\xC3"); mu.mem_write(END, b"\xF4")

OBJ = 0x300
vt = HEAP
vtab = [RET0_STUB] * 0x80
vtab[0xc // 4] = 0x4707E0; vtab[0x14 // 4] = 0x4707C0; vtab[0x1c // 4] = 0x470780
vtab[0x60 // 4] = IS_STUB; vtab[0xec // 4] = 0x661AC0; vtab[0x10c // 4] = 0x470460
mu.mem_write(vt, struct.pack("<128I", *vtab))
arr = HEAP + 0x1000
objbase = HEAP + 0x10000
ptrs = [0] * 0x230
for k, t in types.items():
    o = objbase + k * OBJ
    ptrs[k] = o
    mu.mem_write(o, struct.pack("<II", vt, k))
    mu.mem_write(o + 0x1E4, struct.pack("<I", bits(t["mask"], True)))
    mu.mem_write(o + 0x218, struct.pack("<i", -1))
    mu.mem_write(o + 0x278, struct.pack("<i", 4))
    mu.mem_write(o + 0x2B4, struct.pack("<I", bits(t["flags"], False)))
mu.mem_write(arr, struct.pack(f"<{len(ptrs)}I", *ptrs))
ptrarray = HEAP + 0x2000
mu.mem_write(ptrarray + 0x10, struct.pack("<I", arr))
mu.mem_write(0xC061F8, struct.pack("<I", ptrarray))
consts = HEAP + 0x3000
mu.mem_write(0xC061F0, struct.pack("<I", consts))
balance_this = HEAP + 0x4000
lin_cache = {}

def hook(mu, addr, size, _):
    if addr in (IS_STUB, RET0_STUB):
        this = mu.reg_read(UC_X86_REG_ECX)
        if addr == IS_STUB:
            esp = mu.reg_read(UC_X86_REG_ESP)
            n = struct.unpack("<i", mu.mem_read(esp + 4, 4))[0]
            k = (this - objbase) // OBJ
            if k not in lin_cache: lin_cache[k] = lineage(k)
            mu.reg_write(UC_X86_REG_EAX, 1 if n in lin_cache[k] else 0)
        else:
            mu.reg_write(UC_X86_REG_EAX, 0)
mu.hook_add(UC_HOOK_CODE, hook, begin=STUB, end=STUB + 0x1F)

def type_damage(a, t):
    esp = STACK + 0x80000
    esp -= 4; mu.mem_write(esp, struct.pack("<I", t))
    esp -= 4; mu.mem_write(esp, struct.pack("<I", a))
    esp -= 4; mu.mem_write(esp, struct.pack("<I", END))
    mu.reg_write(UC_X86_REG_ESP, esp); mu.reg_write(UC_X86_REG_EBP, 0)
    mu.reg_write(UC_X86_REG_ECX, balance_this)
    mu.emu_start(TYPE_DAMAGE, END, count=200000)
    return struct.unpack("<i", struct.pack("<I", mu.reg_read(UC_X86_REG_EAX)))[0]

if __name__ == "__main__":
    att = sys.argv[1].split(",")
    tgt = sys.argv[2].split(",")
    print("attacker".ljust(24) + "".join(x[:12].rjust(13) for x in tgt))
    for a in att:
        row = [type_damage(byname[a], byname[x]) for x in tgt]
        print(a.ljust(24) + "".join(f"{v}%".rjust(13) for v in row))
