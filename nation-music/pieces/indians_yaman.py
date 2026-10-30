"""Indians, Classical to Enlightenment ages, economy: Raga Yaman.

An evening raga of Hindustani music, in the form of a gat: the sitar plays the composition,
the bansuri flute answers, the tanpura drones Pa-Sa-Sa-Sa, and the tabla keeps teental, the
16-beat cycle. Yaman's scale is the one with the raised fourth (C D E F# G A B). Original gat.
"""
from kit import BONGO_HI, BONGO_LO, CONGA_HI, CONGA_LO, CONGA_MUTE, Piece

TITLE = "Yaman"
NATION, AGES, MOOD = "Indians", (1, 2, 3, 4), "ECONOMIC"

A = ("E5:1 F#5:0.5 G5:0.5 A5:1 G5:1 | F#5:1 E5:0.5 D5:0.5 E5:2 | B4:1 D5:1 E5:1 F#5:1 | G5:2 F#5:2 | "
     "E5:1 F#5:0.5 A5:0.5 B5:1 A5:1 | G5:1 F#5:1 E5:2 | D5:1 E5:0.5 D5:0.5 C5:1 B4:1 | C5:4")
B = ("C6:1 B5:1 A5:1 B5:1 | C6:2 B5:1 A5:1 | G5:0.5 A5:0.5 B5:1 A5:1 G5:1 | F#5:1 E5:1 F#5:2 | "
     "A5:1 G5:0.5 F#5:0.5 E5:1 F#5:1 | G5:1 A5:1 B5:2 | A5:1 G5:0.5 F#5:0.5 E5:1 D5:1 | C5:4")
# teental: dha dhin dhin dha | dha dhin dhin dha | dha tin tin ta | ta dhin dhin dha
DHA = [(BONGO_HI, 70), (CONGA_LO, 84)]
DHIN = [(BONGO_HI, 64), (CONGA_LO, 60)]
TIN = [(BONGO_HI, 60)]
TA = [(CONGA_MUTE, 66)]
TEENTAL = [DHA, DHIN, DHIN, DHA, DHA, DHIN, DHIN, DHA, DHA, TIN, TIN, TA, TA, DHIN, DHIN, DHA]


def build():
    p = Piece(84, 4)
    sitar = p.track("sitar", 104, gain=0.9, pan=-0.15, ring=1.4)
    bansuri = p.track("bansuri", 73, gain=0.9, pan=0.15, vib=(22, 5, 0.35), legato=0.97, rev=0.35)
    tanpura = p.track("tanpura", 104, gain=0.5, rev=0.5, ring=3.0)
    tabla = p.track("tabla", 0, drums=True, gain=0.45, pan=0.1, rev=0.15)
    p.strain("A", A, ["C"] * 8)
    p.strain("B", B, ["C"] * 8)
    # the tanpura's endless cycle: Pa Sa Sa Sa (low)
    p.add(lambda ch, a, n, lev: [tanpura.add(a + i, 1, k, 50) for i, k in enumerate((55, 60, 60, 48))], lo=0)

    def strain(name, leads, lev):
        if lev >= 2:  # teental under the strain: one 16-beat cycle every four bars
            for bar in range(8):
                for beat, bols in enumerate(TEENTAL[(bar % 4) * 4:(bar % 4) * 4 + 4]):
                    a = p.t + bar * 4 + beat
                    for key, vel in bols:
                        tabla.add(a, 0.5, key, vel)
                    if lev >= 3:  # the fast fillers
                        tabla.add(a + 0.5, 0.25, BONGO_LO, 46)
                        tabla.add(a + 0.75, 0.25, BONGO_HI, 42)
        p.play(name, leads, lev)

    # alap: the bansuri alone over the drone, free and slow
    bansuri.play("G4:2 B4:g C5:3 E5:g D5:1 E5:2 | F#5:2 E5:1 D5:1 C5:4", 0, 70)
    p.groove(["C"] * 4, 0)
    strain("A", [(sitar, 86)], 1)
    strain("A", [(sitar, 88)], 2)
    strain("B", [(bansuri, 86), (sitar, 66, -12)], 2)
    strain("A", [(bansuri, 86), (sitar, 70)], 2)
    strain("B", [(sitar, 90), (bansuri, 76, 12)], 3)
    strain("A", [(sitar, 92), (bansuri, 80)], 3)
    p.ritard(2, 64)
    p.final([(sitar, "C4+G4+C5:4", 80), (bansuri, "C5:4", 74), (tanpura, "G3:1 C4 C4 C3:1", 50)])
    tabla.add(p.t, 1, CONGA_LO, 90)
    tabla.add(p.t, 1, BONGO_HI, 80)
    return p.s
