"""Maya, Ancient to Medieval ages, economy: Temple of the Jungle.

The Bonampak murals (8th century) show a Maya orchestra: turtle-shell drums struck with
antlers, the tunkul slit drum, gourd rattles, long wooden trumpets and clay flutes. Here clay
flutes and ocarina over the tunkul and turtle shells, in the green dusk of the rain forest.
G minor pentatonic (G Bb C D F). Original.
"""
from kit import CLAVES, MARACAS, Piece, drone

TITLE = "Temple"
NATION, AGES, MOOD = "Maya", (0, 1, 2), "ECONOMIC"

A = ("D5:2 F5:1 D5:1 | C5:1.5 Bb4:0.5 G4:2 | Bb4:1 C5:1 D5:1 F5:1 | G5:3 F5:1 | "
     "D5:1 F5:1 G5:1 Bb5:1 | G5:1.5 F5:0.5 D5:2 | C5:1 D5:0.5 C5:0.5 Bb4:1 C5:1 | G4:4")
B = ("G5:2 Bb5:1 G5:1 | F5:2 D5:2 | F5:1 G5:1 F5:1 D5:1 | C5:3 Bb4:1 | "
     "C5:1 D5:1 F5:2 | G5:1 F5:1 D5:2 | C5:1 Bb4:0.5 C5:0.5 Bb4:1 G4:1 | G4:4")


def build():
    p = Piece(72, 4)
    oca = p.track("ocarina", 79, gain=0.9, pan=0.15, vib=(12, 5, 0.35), legato=0.95, rev=0.45)
    flute = p.track("clay flute", 75, gain=0.85, pan=-0.15, vib=(14, 4.8, 0.35), legato=0.95, rev=0.45)
    tunkul = p.track("tunkul", 117, gain=0.45, rev=0.3)
    shell = p.track("turtle shell", 0, drums=True, gain=0.3, pan=0.25, rev=0.2)
    rattle = p.track("gourd rattle", 0, drums=True, gain=0.22, pan=-0.3)
    rain = p.track("rain", 122, bank=1, gain=0.12, legato=1.0)
    pad = p.track("drone", 89, gain=0.12, rev=0.5, legato=1.0)
    p.strain("A", A, ["Gm"] * 8)
    p.strain("B", B, ["Gm"] * 8)
    p.add(drone(pad, ["G2", "D3"], 40), lo=1)
    p.perc(tunkul, [(0, 43, 82), (1.5, 50, 64), (2, 43, 72), (3, 50, 60), (3.5, 48, 56)], lo=1)
    p.perc(shell, [(0.5, CLAVES, 54), (2.5, CLAVES, 58), (3.25, CLAVES, 48)], lo=2)
    p.perc(rattle, [(i * 0.5, MARACAS, 54 if i % 2 == 0 else 36) for i in range(8)], lo=2)

    rain.add(0, 10, 60, 64)
    p.groove([None] * 2, 1)
    p.play("A", [(flute, 82)], 1)
    p.play("B", [(oca, 82)], 2)
    p.play("A", [(oca, 84), (flute, 64, -12)], 2)
    p.play("B", [(flute, 86), (oca, 70, 12)], 2)
    rain.add(p.t, 10, 60, 60)
    p.ritard(2, 56)
    p.final([(flute, "G4:5", 72), (pad, "G2+D3:5", 40), (tunkul, "G2:1", 82)])
    return p.s
