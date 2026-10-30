"""Japan, Ancient and Classical ages, economy: Gagaku.

Gagaku, the court music of the Heian period and the oldest orchestral music still played:
the sho mouth organ holding its shimmering clusters, the hichiriki (a double reed) and the
ryuteki flute on the tune, the gakuso zither and the biwa marking the beats, the kakko and
the great taiko slowly. Hyojo mode on E (E F# G A B C# D). Original.
"""
from kit import WOOD_HI, Piece

TITLE = "Gagaku"
NATION, AGES, MOOD = "Japanese", (0, 1), "ECONOMIC"

A = ("E5:2 F#5:1 A5:1 | B5:3 A5:1 | F#5:2 E5:2 | B4:4 | "
     "E5:1 F#5:1 A5:1 B5:1 | C#6:2 B5:2 | A5:1 F#5:1 E5:2 | E5:4")
B = ("B5:2 C#6:1 B5:1 | A5:2 F#5:2 | A5:1 B5:1 A5:1 F#5:1 | E5:4 | "
     "F#5:2 A5:2 | B5:1 A5:1 F#5:2 | E5:1 D5:1 B4:2 | E5:4")
# the sho's aitake clusters, one per bar
SHO_A = ["E5+F#5+A5+B5", "F#5+A5+B5+E6", "E5+F#5+A5+B5", "B4+E5+F#5+B5",
         "E5+F#5+A5+B5", "A5+B5+C#6+E6", "F#5+A5+B5+E6", "E5+F#5+A5+B5"]
SHO_B = ["F#5+A5+B5+E6", "E5+F#5+A5+B5", "F#5+A5+B5+E6", "E5+F#5+A5+B5",
         "F#5+A5+B5+E6", "E5+F#5+A5+B5", "B4+D5+E5+B5", "E5+F#5+A5+B5"]


def build():
    p = Piece(48, 4)
    hichi = p.track("hichiriki", 68, gain=0.9, pan=-0.1, vib=(24, 4, 0.5), legato=0.98, rev=0.5)
    ryu = p.track("ryuteki", 73, gain=0.7, pan=0.15, vib=(18, 4.5, 0.5), legato=0.98, rev=0.5)
    sho = p.track("sho", 22, gain=0.35, rev=0.55, legato=1.0)
    gakuso = p.track("gakuso", 107, gain=0.6, pan=-0.25, ring=2.0, rev=0.45)
    biwa = p.track("biwa", 106, gain=0.5, pan=0.3, ring=1.6, rev=0.4)
    taiko = p.track("taiko", 116, gain=0.6, rev=0.5)
    kakko = p.track("kakko", 0, drums=True, gain=0.3, rev=0.35)
    p.strain("A", A, [None] * 8)
    p.strain("B", B, [None] * 8)

    def section(name, leads, sho_line, lev):
        start = p.t
        sho.play(" ".join(f"{c}:4" for c in sho_line), start, 52)
        for i in range(8):
            a = start + i * 4
            gakuso.play("E3+B3:0.25 E4:1.75 B3:2", a, 56)
            if lev >= 2:
                biwa.play("r:2 E3+B3+E4:2", a, 58)
                taiko.add(a + 3, 1, 40, 84)
                # the kakko's roll that speeds up into the drum
                for j, d in enumerate((0, 0.5, 0.85, 1.1, 1.3, 1.45, 1.55)):
                    kakko.add(a + 1 + d, 0.1, WOOD_HI, 48 + j * 4)
        p.play(name, leads, lev)

    sho.play("E5+F#5+A5+B5:8", 0, 48)
    p.t += 4
    section("A", [(hichi, 80)], SHO_A, 1)
    section("B", [(hichi, 82), (ryu, 64, 12)], SHO_B, 2)
    section("A", [(ryu, 80, 12), (hichi, 74)], SHO_A, 2)
    section("B", [(hichi, 84), (ryu, 70, 12)], SHO_B, 2)
    p.ritard(2, 38)
    p.final([(sho, "E5+F#5+A5+B5:6", 50), (hichi, "E5:5", 70), (gakuso, "E3:0.25 B3 E4 F#4 B4:5", 60), (taiko, "E2:2", 90)])
    return p.s
