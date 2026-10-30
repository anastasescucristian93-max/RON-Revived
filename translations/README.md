# Translations

The game in ten languages: its own five (English, German, Spanish, French, Italian) plus Russian, Brazilian Portuguese, Polish, Turkish and Romanian. Every text our mods add or change is in all ten.

Status: requested 2026-10-29. Done the same day, not yet tried in game: the five new languages in the options (`camera-smooth/src/lang.c`); every text of the 13 interface files and of `help.xml` (tooltips, encyclopedia) in the five new languages, about 9,500 texts each; the mods' texts and the DLL's own texts in all nine. Left English on purpose: console commands, file paths and internal names the game's own translations also keep. Short texts measured against the widest translation the game ships (`fitcheck.py`): after a shortening pass, Russian 507, Polish 238, Turkish 115, Portuguese 75 and Romanian 53 are still wider than that, most by under 15 % (names and words with no shorter form). Whether they fit the real boxes needs a look in game.

## How the game picks a language

Options > Player > Language, or the round flag button under Workshop in the launcher (saved as `Language` in `%APPDATA%\Microsoft Games\Rise of Nations\rise2.ini`; the first time the game follows Steam's language). For every XML file it opens, the game first looks for `<file>.xml.<id>`, where id is the language's Windows number: German 7, Spanish 10, French 12, Italian 16, and ours: Russian 25, Portuguese 22, Polish 21, Turkish 31, Romanian 24. If that file is not there it uses `<file>.xml`, which is English. The exe only knew five languages; `lang.c` gives it ten (`more_languages` under `[ui]` in `ron_camera.ini`). Engine notes: `camera-smooth/ENGINE_NOTES.md`, "Languages".

The game's fonts (Arial, Georgia) have the letters of all ten languages. Chinese, Japanese and Korean translations ship with the game but are left out: the fonts have no glyphs for them.

## Why the game's own four languages are here too

Our mods replace `help.xml`, `typenames.xml` and `options.xml` with English copies. A German player's game used to load the original `help.xml.7`, so the mods' new names, nation powers and buttons were missing or wrong. Now each of those files gets a copy per language: our mods' file, with the texts the mods did not touch taken from the game's own translation and the texts they changed in that language.

## Files

| Path | What it is |
|---|---|
| `text/<code>.json` | The translations, English text -> translation, one file per language. This is the source; everything else is built from it. |
| `launcher.json` | The launcher's texts (its four buttons, the Workshop agreement, the Update plate's `ron_update_*` texts) in all ten languages; built into `mod_override\launcher_res\locale\<code>.ini` for the launcher's language button (`camera-smooth/src/launcher_lang.c`). |
| `dll_strings.txt` | The DLL's own texts (assistant panel, ranks, button help, PLAY), exactly as in the C code. |
| `TRANSLATING.md` | The rules for translating a chunk (tokens, length, names, style per language). |
| `tools/langs.py` | The languages, the game files and the data mods' order. |
| `tools/xmltext.py` | The translatable texts of an XML file, matched across languages by their path. |
| `tools/extract.py` | Makes the chunks to translate (`work/chunks/`, not in git) and the glossaries of names. |
| `tools/merge.py` | Checks a translated chunk (`check`) and adds the good answers to `text/<code>.json`. |
| `tools/fitcheck.py` | Measures the short texts in the game's fonts and lists those that may not fit their box. |
| `tools/build_data.py` | Builds `mod_override/Data/`: the per-language files and `revived_strings.xml.<id>` for the DLL. |
| `mod_override/` | Built; goes into the game's `mod_override\`. |

## Building

Build every other data mod first (the order in the main README), then:

```
cd translations
python tools/build_data.py "<game folder>" mod_override
```

It prints, per language and file, how many texts are translated and how many are still English.

## After changing a text in a mod

1. Rebuild that mod, then `python tools/extract.py mods` (from `tools/`): a chunk with only the texts that have no translation yet.
2. Translate it for each language (`TRANSLATING.md`), `python tools/merge.py <code>`, `python tools/fitcheck.py <code>`.
3. Rebuild the translations.

A new text in the DLL also needs its line in `dll_strings.txt`, and the C code must pass it through `lang_text()` (and `lang_string()` to make the game String).

## To check in game

- The launcher: a round flag under Workshop; a click opens the other nine around it, a pick changes the launcher's buttons at once and the game starts in that language. Check that Workshop stays clickable, that the window can still be dragged with the button following it, and that the Workshop agreement shows in the chosen language.

- Options > Player > Language lists ten languages; the five new names show correctly (Русский, Português (Brasil), Polski, Türkçe, Română). After a change the game may need a restart.
- In each new language: the main menu, the setup screen, the in-game panels and tooltips, a unit's name. Look for letters drawn as boxes or question marks, and for text cut off or running out of its box.
- In German (or another of the game's languages): the mods' buildings (the city districts, the Field Fortification), the nation powers in the setup screen, the assistant panel, a unit's rank in its tooltip.
- `ron_camera.log`: "languages: 10 in the options ..." and, in a translated language, "languages: N of the mod's texts in ...".
