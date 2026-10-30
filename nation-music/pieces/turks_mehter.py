"""Turks, Medieval to Enlightenment ages, battle: Mehter.

The mehter, the Janissary band, the oldest military band in the world: zurnas (shawms) on
the tune, borus (trumpets), the davul (big drum), the kos (giant kettledrums carried on
camels), the nakkare (small kettledrums) and the zil cymbals. Its sound reached Mozart and
Beethoven as "Turkish music". Slow and heavy, in makam Hicaz on G (G Ab B C D Eb F). Original.
"""
from kit import CRASH, CRASH2, KICK, STICK, Piece, drone

TITLE = "Mehter"
NATION, AGES, MOOD = "Turks", (2, 3, 4), "BATTLE_VICTORY"

A = ("G4:1 G4:0.5 Ab4:0.5 B4:1 C5:1 | D5:2 C5:1 B4:1 | C5:1 D5:0.5 Eb5:0.5 D5:1 C5:1 | B4:1 Ab4:1 G4:2 | "
     "D5:1 D5:0.5 Eb5:0.5 F5:1 Eb5:1 | D5:1 C5:0.5 B4:0.5 C5:2 | B4:1 C5:0.5 B4:0.5 Ab4:1 B4:1 | G4:4")
B = ("G5:2 F5:1 Eb5:1 | D5:2 C5:2 | Eb5:1 D5:1 C5:1 B4:1 | C5:2 D5:2 | "
     "G5:1 F5:0.5 Eb5:0.5 D5:1 Eb5:1 | F5:1 Eb5:0.5 D5:0.5 C5:2 | B4:1 C5:1 Ab4:1 B4:1 | G4:4")


def build():
    p = Piece(96, 4)
    zurna = p.track("zurnas", 111, gain=0.9, pan=-0.1, vib=(10, 6, 0.2), legato=0.92)
    zurna2 = p.track("zurnas 2", 111, gain=0.6, pan=0.2, vib=(10, 5.6, 0.25), legato=0.92)
    boru = p.track("borus", 56, gain=0.55, rev=0.4, legato=1.0)
    kos = p.track("kos", 47, gain=0.6, rev=0.35)
    davul = p.track("davul", 116, gain=0.85, rev=0.25)
    tek = p.track("davul rim", 0, drums=True, gain=0.4, pan=0.15)
    nakkare = p.track("nakkare", 117, gain=0.4, pan=-0.25)
    zil = p.track("zil", 0, drums=True, gain=0.4, pan=0.3, rev=0.3)
    p.strain("A", A, ["G5"] * 8)
    p.strain("B", B, ["G5"] * 8)
    p.add(drone(boru, ["G3"], 64), lo=2)
    # usul: DUM on 1 and 3, tek between, the kos under the first beat
    p.perc(davul, [(0, 40, 116), (2, 40, 108)], lo=1)
    p.perc(tek, [(1, STICK, 80), (3, STICK, 84), (3.5, STICK, 72)], lo=1)
    p.perc(kos, [(0, 43, 100)], lo=2)
    p.perc(nakkare, [(0.5, 62, 64), (1.5, 62, 70), (2.5, 62, 64), (3.5, 64, 72)], lo=2)
    p.perc(zil, [(1, CRASH, 70), (3, CRASH2, 74)], lo=2)

    boru.play("G3:4 D4:4", 0, 92)
    p.groove([None] * 2, 1)
    p.play("A", [(zurna, 92)], 1)
    p.play("A", [(zurna, 94), (zurna2, 78, 12)], 2)
    p.play("B", [(zurna, 94), (zurna2, 78, -12)], 2)
    p.play("B", [(zurna, 96), (zurna2, 82, 12)], 2)
    boru.play("G3:2 D4:2", p.t, 96)
    p.roll(nakkare, 62, 4, 60, 116)
    p.groove([None], 1)
    p.play("A", [(zurna, 100), (zurna2, 86, 12)], 3)
    p.ritard(2, 80)
    p.final([(davul, "E2:1 E2:1 E2:2", 124), (kos, "G2:4", 110), (boru, "G3+D4:4", 98), (zurna, "G4:4", 100)])
    zil.add(p.t + 2, 3, CRASH, 110)
    return p.s
