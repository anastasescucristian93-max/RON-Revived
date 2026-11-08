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

Brightened 2026-11-08 (the user: too dark; brightness of the building pixels 69 against the original's 110): `brighten_sprite.py` gamma 0.62, warmth 0 (colours kept) on the installed sprite, brightness 110 now. The darker one is kept as `work/stable_6_age8_before_brighten.tga`, the comparison in `work/brighten_compare.png`; the site picture was exported again with `site_image.py`. The same day the shadow went too (the user, seen in game: a dark band on the ground at the lower left): its 2113 shadow pixels (alpha 145, from `make_sprite.py --shadow`) set to transparent, written again with `write_like.py`. Then moved 20 px lower (the user, seen in game: its corners should reach the corners of the grey foundation): the canvas grown 20 rows at the bottom (397 x 317, `write_like.py --any-size`; a canvas grown only downwards needs no offset in `nation_art.ini`), the building pasted 20 px down, the depth mesh made again (`depth_mesh.py stable_6`, 92 % covered). In `footprint_check.py` its wall corners now sit on the footprint's left and right corners (overflow 12.6 at the front, from the crates and robots). The copy before the move is `work/stable_6_age8_before_lower.tga`.

```
python ../roman-buildings/tools/brighten_sprite.py nation_art/all/stable_6_age8.tga work/lit.png 0.62 0
python ../roman-buildings/tools/write_like.py work/lit.png for-gemini/stable_6__original.tga nation_art/all/stable_6_age8.tga
```

The footprint check found no need to scale it: overflow 11.2 px against the original's 9.8. The depth mesh covers 97 % of the sprite.
