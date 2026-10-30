# Modern buildings (every nation)

New art for buildings that every nation shares in the late ages, where the game's own look no longer fits the age. The sprites live in `nation_art/all/` and are swapped in by the DLL through the `[all]` section of `roman-buildings/nation_art.ini`.

| Building | Original | Ages | Rule |
|---|---|---|---|
| Auto Plant | `stable_6.tga` (ages 6-8) | 8 | `stable_6.tga@8 = nation_art\all\stable_6_age8.tga` |

## Folders

- `for-gemini/`: the original sprite exported for reference (`export_sprites.py`) and its magenta copy given to Gemini.
- `gemini/`: Gemini's pictures. `stable_6_age8_military.jpg` is the one used; `stable_6_age8_service_alt.jpg` is the civilian variant of the same building (it has painted text), kept as an alternative.
- `clean/`: the finished sprite. Since 2026-10-28 no wear is added; the installed Auto Plant still has the wear it got on 2026-10-28, its unworn copy is here.
- `work/`: intermediate sprites and previews.
- `tools/fix_autoplant.py`: removes the magenta left in the railing, then paints the player-colour bands. The picture had no red, so the bands are drawn where the original has its own: under the hall's eaves and along the annex parapet.

## Auto Plant, age 8: how it was made

The prompt was short: the original sprite and "reimagine this industrial building as being a bit more modern but keep the industrial style. 9 variants". Long prompts with many rules came out worse.

```
python ../roman-buildings/tools/make_sprite.py gemini/stable_6_age8_military.jpg for-gemini/stable_6__original.tga work stable_6_age8 0 --shadow
python tools/fix_autoplant.py work/stable_6_age8_A.tga clean/stable_6_age8.tga
python ../roman-buildings/tools/weather.py clean/stable_6_age8.tga work/stable_6_age8_worn.png 1.0
python ../roman-buildings/tools/write_like.py work/stable_6_age8_worn.png for-gemini/stable_6__original.tga nation_art/all/stable_6_age8.tga
python ../roman-buildings/tools/depth_mesh.py stable_6
```

Since 2026-10-28 new sprites get no wear: skip `weather.py` and write `clean/` straight with `write_like.py`.

The footprint check found no need to scale it: overflow 11.2 px against the original's 9.8. The depth mesh covers 97 % of the sprite.
