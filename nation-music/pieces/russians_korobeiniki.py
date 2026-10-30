"""Russians, Enlightenment and Industrial ages, economy: Korobeiniki.

"The Peddlers", a Russian folk song (on Nekrasov's poem of 1861, public domain). As a
folk band plays it: it starts slow on the bayan (button accordion), then the balalaikas
take it with their tremolo and the whole band speeds up, as Russian dances do. A minor.
"""
from kit import Piece, bass, pluck, tremolo, ROOT, FIFTH

TITLE = "Korobeiniki"
NATION, AGES, MOOD = "Russians", (4, 5), "ECONOMIC"

L1 = ("E5:1 B4:0.5 C5:0.5 D5:1 C5:0.5 B4:0.5 | A4:1 A4:0.5 C5:0.5 E5:1 D5:0.5 C5:0.5 | "
      "B4:1.5 C5:0.5 D5:1 E5:1 | C5:1 A4:1 A4:2")
L2 = ("r:0.5 D5:1 F5:0.5 A5:1 G5:0.5 F5:0.5 | E5:1.5 C5:0.5 E5:1 D5:0.5 C5:0.5 | "
      "B4:1 B4:0.5 C5:0.5 D5:1 E5:1 | C5:1 A4:1 A4:2")
SONG = L1 + " | " + L2
HARM = ["E", "Am", "E", "Am", "Dm", "C", "E", "Am"]


def build():
    p = Piece(84, 4)
    bayan = p.track("bayan", 21, gain=0.9, pan=-0.15, legato=0.9)
    bala = p.track("balalaika", 24, gain=0.8, pan=0.2, ring=1.0)
    bala2 = p.track("balalaika chords", 105, gain=0.4, pan=0.35, legato=0.3)
    domra = p.track("domra", 105, gain=0.7, pan=-0.3)
    strings = p.track("strings", 48, gain=0.35, rev=0.35, legato=0.95)
    contra = p.track("contrabass balalaika", 32, gain=0.6, legato=0.6)
    p.strain("S", SONG, HARM)
    p.add(bass(contra, [(0, ROOT), (2, FIFTH)], low=33, vel=80), lo=1)
    p.add(pluck(bala2, [(1, "c"), (3, "c")], low=57, vel=62, dur=0.2), lo=1)
    p.add(pluck(strings, [(0, "c")], low=57, vel=46, dur=3.8), lo=3)

    p.play("S", [(bayan, 76)], 0)
    p.play("S", [(bayan, 80)], 1)
    p.tempo(100)
    p.play("S", [(bala, 84, tremolo)], 2)
    p.play("S", [(domra, 84, lambda m: tremolo(m, 0.125, 0.5)), (bayan, 66, -12)], 2)
    p.tempo(116)
    p.play("S", [(bayan, 88), (bala, 76, tremolo)], 3)
    p.tempo(132)
    p.play("S", [(domra, 88, lambda m: tremolo(m, 0.125, 0.5)), (bayan, 82), (bala, 76, tremolo)], 3)
    p.tempo(148)
    p.play("S", [(bayan, 92), (domra, 84, lambda m: tremolo(m, 0.125, 0.5)), (strings, 70, -12)], 3)
    p.final([(bayan, "E5:0.5 A4+C5+E5:1! r:0.5", 96), (contra, "E2:0.5 A1:1", 96), (bala2, "r:0.5 A3+C4+E4:1", 90)])
    return p.s
