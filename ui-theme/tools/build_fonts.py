r"""Writes <out>\Data\fonts.xml: the game's fonts.xml with the larger fonts of the game setup
screen added (Georgia 20 bold, Arial 15 bold, in both resolution lists). camera-smooth's
flags.c asks for them (setup_font_percent=125: 12 -> 15, 14 -> 18, 16 -> 20; Georgia 18 bold
is already the game's). The game only has the fonts this file lists; a size it does not list
gives the closest one.

    python build_fonts.py <game folder> <out mod_override folder>
"""
import os
import sys

ADD = ("\t\t\t\t<!-- RON Revived: the game setup screen's fonts 25% larger (ui-theme, flags.c) -->\r\n"
       "\t\t\t\t<FONT filename=\"Data\\Fonts\\georgiab.ttf\" name=\"Georgia\" size=\"20\" style=\"B\"/>\r\n"
       "\t\t\t\t<FONT filename=\"Data\\Fonts\\arialbd.ttf\" name=\"Arial\" size=\"15\" style=\"B\"/>\r\n")


def main(game, out):
    s = open(os.path.join(game, "Data", "fonts.xml"), encoding="utf-8", newline="").read()
    for res in ('<RESOLUTION res="1280">\r\n', '<RESOLUTION res="1024">\r\n'):
        i = s.index(res) + len(res)
        s = s[:i] + ADD + s[i:]
    path = os.path.join(out, "Data", "fonts.xml")
    open(path, "w", encoding="utf-8", newline="").write(s)
    print(path)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
