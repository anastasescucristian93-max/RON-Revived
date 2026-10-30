"""Chinese, Medieval to Enlightenment ages, battle: Ambush Drums.

Chinese armies advanced to the war drum and fell back to the gong. The big dagu drums and the
cymbals drive it, the suona shawm shrieks the tune, the pipa answers with tremolo. D minor
pentatonic (D F G A C). Original.
"""
from kit import CHINA, CRASH2, Piece, drone, tremolo
from score import transpose

TITLE = "AmbushDrums"
NATION, AGES, MOOD = "Chinese", (2, 3, 4), "BATTLE_VICTORY"

A = ("D5:0.5 F5 G5 A5 C6:1 A5:1 | G5:0.5 A5 G5 F5 D5:2 | F5:0.5 G5 A5 C6 D6:1 C6:1 | A5:0.5 C6 A5 G5 A5:2 | "
     "D6:1 C6:0.5 A5 G5:1 A5:1 | C6:0.5 D6 C6 A5 G5:2 | F5:0.5 G5 A5 G5 F5:1 D5:1 | D5:2 r:2")
B = ("A5:1 A5:1 C6:1 D6:1 | F6:2 D6:2 | C6:0.5 D6 C6 A5 G5:2 | A5:4 | "
     "D6:1 C6:1 A5:1 G5:1 | F5:1 G5:1 A5:2 | G5:0.5 A5 G5 F5 D5:1 F5:1 | D5:4")


def build():
    p = Piece(132, 4)
    suona = p.track("suona", 111, gain=0.9, pan=-0.1, legato=0.9)
    pipa = p.track("pipa", 106, gain=0.6, pan=0.25)
    dagu = p.track("dagu", 116, gain=0.9, rev=0.25)
    tang = p.track("tanggu", 116, gain=0.55, pan=0.2, rev=0.2)
    cym = p.track("cymbals", 0, drums=True, gain=0.35, pan=-0.25, rev=0.25)
    gong = p.track("gong", 0, drums=True, gain=0.4, rev=0.5)
    low = p.track("low strings", 48, gain=0.35, rev=0.3, legato=1.0)
    p.strain("A", A, ["D5"] * 8)
    p.strain("B", B, ["D5"] * 8)
    p.add(drone(low, ["D3", "A3"], 56), lo=2)
    p.perc(dagu, [(0, 38, 112), (1, 38, 88), (2, 38, 104), (2.5, 38, 84), (3, 38, 96)], lo=1)
    p.perc(tang, [(0.5, 50, 72), (1.5, 50, 76), (3.5, 50, 84)], lo=2)
    p.perc(cym, [(1, CHINA, 74), (3, CHINA, 80)], lo=2)
    p.perc(gong, [(0, CRASH2, 90)], lo=2, every=4)

    p.roll(dagu, 38, 4, 40, 110, step=0.25)
    p.t += 4
    p.groove([None] * 2, 1)
    # the pipa's tremolo, an octave down under the suona
    low_trem = lambda m: tremolo(transpose(m, -12))
    p.play("A", [(suona, 92)], 2)
    p.play("A", [(suona, 94), (pipa, 70, low_trem)], 2)
    p.play("B", [(pipa, 86, tremolo)], 2)
    p.play("B", [(suona, 96), (pipa, 74, low_trem)], 2)
    p.roll(dagu, 38, 8, 60, 120, step=0.25)
    p.groove([None] * 2, 1)
    p.play("A", [(suona, 100), (pipa, 80, low_trem)], 3)
    p.final([(dagu, "D2:1 D2:1 D2:2", 124), (suona, "D6:3! r:1", 100), (gong, "A3:4", 110)])  # A3 = key 57, the crash as gong
    return p.s
