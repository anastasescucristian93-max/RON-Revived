"""Mongols, Classical to Gunpowder ages, economy: Long Song.

Urtiin duu, the "long song" of the steppe (on UNESCO's heritage list): long held notes with
wide ornaments, the morin khuur horse-head fiddle carrying it, the limbe flute answering,
a low drone in the manner of khoomei throat singing, and in the middle the soft gallop of a
horse. G major pentatonic (G A C D E), slow and free. Original.
"""
from kit import CONGA_LO, CONGA_MUTE, Piece, drone

TITLE = "LongSong"
NATION, AGES, MOOD = "Mongols", (1, 2, 3), "ECONOMIC"

A = ("D5:3 E5:0.5 D5:0.5 | C5:1 A4:1 G4:2 | A4:1 C5:1 D5:1 E5:1 | G5:4 | "
     "E5:1 G5:1 A5:1 G5:1 | E5:1.5 D5:0.5 C5:2 | D5:1 C5:0.5 A4:0.5 G4:1 A4:1 | G4:4")
B = ("G5:2 A5:1 G5:1 | E5:2 D5:2 | E5:1 D5:1 C5:1 A4:1 | C5:4 | "
     "D5:1 E5:1 G5:1 E5:1 | D5:2 C5:2 | A4:1 C5:0.5 A4:0.5 G4:1 E4:1 | G4:4")


def build():
    p = Piece(60, 4)
    khuur = p.track("morin khuur", 42, gain=0.95, pan=-0.1, vib=(28, 5, 0.3), legato=0.98, rev=0.45)
    limbe = p.track("limbe", 73, gain=0.8, pan=0.2, vib=(20, 5, 0.35), legato=0.97, rev=0.45)
    throat = p.track("khoomei drone", 70, gain=0.35, rev=0.4, legato=1.0)
    hum = p.track("voices", 52, gain=0.2, rev=0.5, legato=1.0)
    gallop = p.track("gallop", 0, drums=True, gain=0.3, rev=0.2)
    p.strain("A", A, ["G5"] * 8)
    p.strain("B", B, ["G5"] * 8)
    p.add(drone(throat, ["G2"], 64))
    p.add(drone(hum, ["G3", "D4"], 40), lo=2)
    p.perc(gallop, [(b + o, k, v) for b in range(4) for o, k, v in ((0, CONGA_LO, 66), (1 / 3, CONGA_MUTE, 50),
                                                                   (2 / 3, CONGA_LO, 74))], lo=2)

    # the long song breathes: each line slows into its last note
    def line(name, leads, lev):
        start = p.t
        p.play(name, leads, lev)
        for half in (start + 16, p.t):
            p.s.tempo_at(half - 3, 60)
            p.s.tempo_at(half - 0.5, 46)
            p.s.tempo_at(half + 0.25, 60)

    throat.play("G2:8", 0, 60)
    p.t += 8
    line("A", [(khuur, 82, -12)], 1)
    line("B", [(limbe, 82)], 1)
    line("A", [(limbe, 82), (khuur, 66, -12)], 2)
    line("B", [(khuur, 86, -12), (limbe, 66)], 2)
    line("A", [(khuur, 82, -12)], 1)
    p.final([(khuur, "G3:6", 72), (throat, "G2:6", 64), (limbe, "D5:2 G5:4", 60)])
    return p.s
