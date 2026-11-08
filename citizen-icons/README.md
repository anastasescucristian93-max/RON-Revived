# Citizen icons

The Citizen's icon of the Industrial, Modern and Information Ages (6-8), the worker with the yellow hard hat, now has one look per people. Before, every nation showed the same white European worker.

| Icon | Unit style | Nations |
|---|---|---|
| European | European | Americans, British, Dutch, Egyptians, French, Germans, Greeks, Romans, Russians, Spanish, Turks, Romania |
| African | Arab | Bantu, Nubians |
| East Asian | Asian (+ the Korean Citizen) | Chinese, Japanese, Koreans, Mongols |
| South Asian | Indian | Indians, Persians |
| Native North American | North American | Iroquois, Lakota |
| Indigenous Latin American | American | Aztecs, Inca, Maya |

The game gives each nation the icons of its unit style (`<UNIT_CONTINENT>` in `tribes\<nation>.xml`), so Egyptians and Turks, who use the European style for all their units, get the European icon. Romania shows its Razes icon up to age 6 and this European icon in ages 7-8 (`romania.c`).

## Files

- `for-gemini/`: the game's icon (`citizen_age6-8_original.png`, 61 x 39) and its 8x enlargement given to Gemini.
- `gemini/collage_v2.png`: Gemini's collage, 3 x 2 panels, in the table's order (`collage_v1.png` was a first try, not used).
- `icons/`: the six icons at the game's size; `icons/large/`: the crops they are made from.
- `tools/build_data.py`: cuts the icons, puts them into the icon pages and points the Citizens' graphics entries at them.

## Building

`python tools/build_data.py`, after nuke-fx (it builds on nuke-fx's `iface_units_03.tga` and `unit_graphics.xml`) and before romania, which builds on this mod's `unit_graphics.xml`.

## Gemini prompt

Made in a new chat with only the enlarged game icon:

```
Image 1 is a small game icon of a modern construction worker. Make a collage of 6 versions of this icon in a 3 x 2 grid, each reinterpreted for a different people: 1 European, 2 Sub-Saharan African with dark skin, 3 East Asian, 4 South Asian (Indian), 5 Native North American, 6 Indigenous Latin American (Andean / Mexican). Every worker wears the same yellow hard hat, holds the wrench and wears the work jacket of image 1, in front of the same blurred construction site. Painted in the style of image 1. Calm, confident, ordinary working men; natural skin tones, hands the same tone as the face. No caricature, no folk costume. Thin white gaps, no text.
```

Then, in the same chat, panel 5 had come out orange-red:

```
Keep the collage exactly as it is. Only panel 5 (middle of the bottom row): give him a natural brown skin tone like panel 6, not orange or red. Change nothing else.
```

Lessons: Gemini dropped the hard hat on some panels unless the prompt says every worker wears it, painted pale hands under dark faces unless told to match them, and tends to paint Native Americans orange-red. Asking for a "rougher, tired" look made them all look worn out, so the user started again.
