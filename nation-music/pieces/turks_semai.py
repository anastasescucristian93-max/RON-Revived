"""Turks, Medieval to Enlightenment ages, economy: Saz Semai.

Ottoman court music: the ney flute of the Mevlevi dervishes, the oud, the kanun zither and
the darbuka, in the 10/8 aksak semai usul (3+2+2+3) that closes an Ottoman fasil. Makam
Nihavend on D (D E F G A Bb C#), the Turkish harmonic minor. Original.
"""
from kit import BONGO_HI, CONGA_LO, CONGA_MUTE, Piece, arp, drone, tremolo

TITLE = "SazSemai"
NATION, AGES, MOOD = "Turks", (2, 3, 4), "ECONOMIC"

A = ("D5:1.5 E5:0.5 F5:1 G5:1 A5:1 | Bb5:1.5 A5:0.5 G5:1 F5:1 E5:1 | F5:1.5 E5:0.5 D5:1 C#5:1 E5:1 | D5:5 | "
     "A5:1.5 Bb5:0.5 A5:1 G5:1 F5:1 | G5:1.5 F5:0.5 E5:1 F5:1 G5:1 | A5:1 G5:0.5 F5:0.5 E5:1 C#5:1 E5:1 | D5:5")
B = ("A5:1.5 C6:0.5 D6:1 C6:1 Bb5:1 | A5:1.5 Bb5:0.5 A5:1 G5:1 F5:1 | G5:1.5 A5:0.5 Bb5:1 A5:1 G5:1 | F5:2 E5:3 | "
     "D6:1.5 C6:0.5 Bb5:1 A5:1 G5:1 | F5:1.5 G5:0.5 A5:1 F5:1 E5:1 | F5:1 E5:0.5 D5:0.5 E5:1 C#5:1 E5:1 | D5:5")


def build():
    p = Piece(108, 5)
    ney = p.track("ney", 77, gain=0.95, pan=0.1, vib=(16, 4.5, 0.35), legato=0.97, rev=0.45)
    oud = p.track("oud", 24, gain=0.75, pan=-0.2, ring=1.1)
    kanun = p.track("kanun", 46, gain=0.5, pan=0.3, ring=1.2)
    drone_ = p.track("drone", 49, gain=0.15, rev=0.5, legato=1.0)
    darbuka = p.track("darbuka", 0, drums=True, gain=0.45, rev=0.15)
    p.strain("A", A, ["Dm", "Gm", "A", "Dm", "Dm", "Gm", "A", "Dm"])
    p.strain("B", B, ["Dm", "Dm", "Gm", "A", "Gm", "Dm", "A", "Dm"])
    p.add(arp(kanun, 0.5, order=(0, 2, 1, 3, 2, 4, 3, 2, 1, 0), low=50, vel=46, accent=12, every=10), lo=2)
    p.add(drone(drone_, ["D3", "A3"], 40), lo=1)
    # aksak semai: dum (3) tek (2) dum-dum (2) tek (3), in eighths
    p.perc(darbuka, [(0, CONGA_LO, 90), (1.5, BONGO_HI, 70), (2.5, CONGA_LO, 80), (3, CONGA_LO, 74),
                     (3.5, BONGO_HI, 74), (4.5, CONGA_MUTE, 56)], lo=1)

    oud.play("D4:0.5 A4 D5 F5 E5 D5 C#5 D5 A4:1 | Bb4:0.5 A4 G4 F4 E4 F4 E4 D4:1.5", 0, 70)
    p.t += 10
    p.play("A", [(ney, 82)], 1)
    p.play("A", [(oud, 84, tremolo), (ney, 66, 12)], 2)
    p.play("B", [(ney, 86)], 2)
    p.play("B", [(oud, 84, tremolo), (kanun, 64, 12)], 2)
    p.play("A", [(ney, 88), (oud, 70, tremolo)], 3)
    p.ritard(2, 76)
    p.final([(oud, "D3:0.5 A3 D4 A4:3.5", 80), (ney, "D5:5", 72), (drone_, "D3+A3:5", 40)])
    return p.s
