"""Americans, Enlightenment to Industrial ages, economy: Oh! Susanna.

Stephen Foster's 1848 song (public domain), the tune of the wagon trains and the gold rush,
played the way the frontier played it: banjo, fiddle, harmonica, guitar and an upright bass
keeping the "boom-chick". C major, 2/4.
"""
from kit import FIFTH, ROOT, Piece, bass, pluck

TITLE = "Susanna"
NATION, AGES, MOOD = "Americans", (4, 5, 6), "ECONOMIC"

VERSE = ("E5:0.5 G5:0.5 G5:0.75 A5:0.25 | G5:0.5 E5:0.5 C5:0.75 D5:0.25 | E5:0.5 E5:0.5 D5:0.5 C5:0.5 | "
         "D5:1.5 C5:0.25 D5:0.25 | E5:0.5 G5:0.5 G5:0.75 A5:0.25 | G5:0.5 E5:0.5 C5:0.75 D5:0.25 | "
         "E5:0.5 E5:0.5 D5:0.5 D5:0.5 | C5:1.5 C5:0.25 D5:0.25")
CHORUS = ("F5:1 F5:1 | A5:1 A5:1 | G5:0.5 G5:0.5 E5:0.5 C5:0.5 | D5:1.5 C5:0.25 D5:0.25 | "
          "E5:0.5 G5:0.5 G5:0.75 A5:0.25 | G5:0.5 E5:0.5 C5:0.75 D5:0.25 | E5:0.5 E5:0.5 D5:0.5 D5:0.5 | "
          "C5:1.5 C5:0.25 D5:0.25")
# the banjo's way with the verse: rolls of sixteenths around the tune
VERSE_ROLL = ("E5:0.25 C5 G5 C5 G5 E5 A5 G5 | G5:0.25 C5 E5 C5 C5 G4 D5 C5 | E5:0.25 C5 E5 C5 D5 B4 C5 G4 | "
              "D5:0.25 B4 G4 B4 D5 G5 C5 D5 | E5:0.25 C5 G5 C5 G5 E5 A5 G5 | G5:0.25 C5 E5 C5 C5 G4 D5 C5 | "
              "E5:0.25 C5 E5 C5 D5 B4 D5 B4 | C5:0.25 G4 E4 G4 C5 E5 C5 D5")


def build():
    p = Piece(112, 2)
    banjo = p.track("banjo", 105, gain=0.9, pan=-0.2, ring=1.2)
    fiddle = p.track("fiddle", 110, gain=0.95, pan=0.2, vib=(10, 6, 0.2), legato=0.85)
    harp = p.track("harmonica", 22, gain=0.8, pan=0.05, legato=0.9)
    gtr = p.track("guitar", 25, gain=0.45, pan=0.35, legato=0.3)
    ubass = p.track("bass", 32, gain=0.6, legato=0.6)
    p.strain("V", VERSE, ["C", "C", "C", "G", "C", "C", "G", "C"])
    p.strain("C", CHORUS, ["F", "F", "C", "G", "C", "C", "G", "C"])
    p.strain("R", VERSE_ROLL, ["C", "C", "C", "G", "C", "C", "G", "C"])
    p.add(bass(ubass, [(0, ROOT), (1, FIFTH)], low=36))
    p.add(pluck(gtr, [(0.5, "c"), (1.5, "c")], low=55))

    p.groove(["C", "G", "C", "G"], 1)
    p.final([(banjo, "C5:0.5 D5:0.5", 76)], at=p.t - 1)
    for strain, leads in (("V", [(banjo, 84)]), ("C", [(banjo, 84)]),
                          ("V", [(fiddle, 86)]), ("C", [(fiddle, 86), (banjo, 62)]),
                          ("R", [(banjo, 80)]), ("C", [(harp, 84)]),
                          ("V", [(harp, 80), (fiddle, 70, -12)]), ("C", [(fiddle, 90), (harp, 76), (banjo, 66)])):
        p.play(strain, leads, 2)
    p.ritard(2, 92)
    p.final([(fiddle, "C5:1! C4+G4:1", 90), (banjo, "C4+E4+G4+C5:1! r:1", 84), (harp, "C5+E5:2", 76),
             (ubass, "C2:1", 90)])
    return p.s
