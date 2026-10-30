"""Chinese, Medieval to Industrial ages, economy: Mo Li Hua (Jasmine Flower).

The Jiangnan folk song (traditional, Qing dynasty, public domain; Puccini used it in Turandot)
on the instruments of a silk-and-bamboo ensemble: guzheng zither, dizi flute, erhu fiddle and
yangqin dulcimer. C major pentatonic (C D E G A).
"""
from kit import Piece, arp, drone

TITLE = "Jasmine"
NATION, AGES, MOOD = "Chinese", (2, 3, 4, 5), "ECONOMIC"

SONG = ("E5:1 E5:0.5 G5:0.5 A5:0.5 C6:0.5 C6:0.5 A5:0.5 | G5:1 G5:0.5 A5:0.5 G5:2 | "
        "E5:1 E5:0.5 G5:0.5 A5:0.5 C6:0.5 C6:0.5 A5:0.5 | G5:1 G5:0.5 A5:0.5 G5:2 | "
        "G5:1 G5:1 G5:1 E5:0.5 G5:0.5 | A5:1 A5:1 G5:2 | "
        "E5:1 D5:0.5 E5:0.5 G5:1 E5:0.5 D5:0.5 | C5:1 C5:0.5 D5:0.5 C5:2 | "
        "E5:0.5 D5:0.5 C5:0.5 E5:0.5 D5:1.5 E5:0.5 | G5:1 A5:0.5 C6:0.5 G5:2 | "
        "D5:1 E5:0.5 G5:0.5 D5:0.5 E5:0.5 C5:0.5 A4:0.5 | G4:2 r:2 | "
        "A4:1 C5:1 D5:1.5 E5:0.5 | C5:0.5 D5:0.5 C5:0.5 A4:0.5 G4:2")
HARM = ["C", "C", "C", "C", "C", "Am", "C", "C", "C", "C", "G", "G", "Am", "C"]


def build():
    p = Piece(70, 4)
    zheng = p.track("guzheng", 107, bank=8, gain=0.9, pan=-0.2, ring=1.8, rev=0.35)
    dizi = p.track("dizi", 73, gain=0.95, pan=0.1, vib=(16, 5.5, 0.25), legato=0.95)
    erhu = p.track("erhu", 110, gain=0.85, pan=0.2, vib=(30, 5.5, 0.15), legato=0.97)
    yang = p.track("yangqin", 15, gain=0.45, pan=0.3, ring=1.2)
    pad = p.track("strings", 49, gain=0.2, rev=0.5, legato=1.0)
    p.strain("S", SONG, HARM)
    p.add(arp(zheng, 0.5, order=(0, 2, 3, 4, 3, 2, 1, 2), low=48, vel=50, accent=10, every=8), lo=1)
    p.add(drone(pad, ["C3", "G3"], 40), lo=2)
    p.add(arp(yang, 0.25, order=(0, 1, 2, 3, 4, 3, 2, 1), low=60, vel=40, accent=8, every=8), lo=3)

    # the guzheng sweeps its strings
    zheng.play("C5:0.25 D5 E5 G5 A5 C6 D6 E6 G6:2 | A5:0.25 G5 E5 D5 C5 A4 G4 E4 C4:2", 0, 66)
    p.t += 8
    p.play("S", [(zheng, 80, 12)], 1)
    p.play("S", [(dizi, 84, 12)], 2)
    p.play("S", [(erhu, 84), (dizi, 66, 12)], 3)
    p.ritard(2, 52)
    p.final([(zheng, "G6:0.25 E6 D6 C6 A5 G5 E5 D5 C5:3", 70), (erhu, "C5:4", 70), (pad, "C3+G3+E4:5", 40)])
    return p.s
