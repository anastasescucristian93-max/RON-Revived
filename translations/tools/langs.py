"""What the translation tools share: the game folder, the languages and the files."""
import os

GAME = os.environ.get("RON_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations")
HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.dirname(HERE)  # translations\
ROOT = os.path.dirname(MOD)

# code -> (file suffix = Windows language id, name); the order of lang.c's tables after English
GAME_LANGS = {"de": (".7", "German"), "es": (".10", "Spanish"), "fr": (".12", "French"), "it": (".16", "Italian")}
NEW_LANGS = {"ru": (".25", "Russian"), "pt": (".22", "Brazilian Portuguese"), "pl": (".21", "Polish"),
             "tr": (".31", "Turkish"), "ro": (".24", "Romanian")}
ALL_LANGS = {**GAME_LANGS, **NEW_LANGS}

# translated now for the new languages (the interface); help.xml follows
INTERFACE = ["typenames.xml", "string_lookup.xml", "translated_strings.xml", "options.xml", "playerprofile.xml",
             "rules_string_lookup.xml", "resourcerules_strings.xml", "alternatenames.xml", "sound.xml",
             "aboutbox.xml", "IME.xml", "patcher_strings.xml"]
LATER = ["help.xml"]
# every file the game has per language; our mods' copies of these need a copy per language too
LOCALIZED = INTERFACE + LATER + ["eula.xml", "credits.xml", "fonts.xml"]

TEXT = os.path.join(MOD, "text")      # the translations: text\<code>.json, English -> translation
WORK = os.path.join(MOD, "work")      # chunks for the translators and their answers
OUT = os.path.join(MOD, "mod_override", "Data")
DLL_STRINGS = os.path.join(MOD, "dll_strings.txt")

# the data mods in their build order (package\build_package.py's MOD_ORDER without us)
MOD_ORDER = ["data-center", "paradrop", "entrenchment", "general-heal", "nuke-fx", "ui-theme", "romania"]


def game_file(name, suffix=""):
    return os.path.join(GAME, "Data", name + suffix)


def mod_file(name):
    """our mods' English copy of a Data file (the last mod in the build order that has one), or None"""
    for mod in reversed(MOD_ORDER):
        p = os.path.join(ROOT, mod, "mod_override", "Data", name)
        if os.path.exists(p):
            return p
    return None
