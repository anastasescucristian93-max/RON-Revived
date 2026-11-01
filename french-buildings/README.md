# French buildings

The French's own building art. Until 2026-10-31 they shared the northern European set for ages 1-3 (`northern-european-buildings`) with the British, Germans and Dutch; the user found it ugly (dark, muddy, ragged shadows, art off its footprint), so the French get their own, one building at a time. The plan and what is left are in `NATIONS.md` (French). The sprites live in `nation_art/french/` and are swapped in by the DLL through the `[french]` section of `roman-buildings/nation_art.ini`.

| Building | Original | Ages | Rule |
|---|---|---|---|
| Temple | `temple_0m.tga` | 1-3 | `temple_0m.tga = nation_art\french\temple_0m.tga` |
| Small City | `village_0ma.tga` | 1-3 | `village_0ma.tga = nation_art\french\village_0ma.tga` |
| Large City | `city_0ma.tga` | 3 | `city_0ma.tga = nation_art\french\city_0ma.tga` |
| Barracks | `barracks_0m.tga` | 1-3 | `barracks_0m.tga = nation_art\french\barracks_0m.tga` |
| Market | `market_0m.tga` | 1-3 | `market_0m.tga = nation_art\french\market_0m.tga` |
| Library | `library_0m.tga` | 1-3 | `library_0m.tga = nation_art\french\library_0m.tga` |

## Folders

- `for-gemini/`: the game's original sprites, for reference and as the canvas for `write_like.py`.
- `gemini/`: Gemini's pictures.
- `clean/`: the finished sprites (no wear).
- `work/`: intermediate sprites and checks.

## Temple, ages 1-3: how it was made

The old northern European church picture (`northern-european-buildings/gemini/temple_age1-3.jpg`) was edited in Gemini with a short prompt: remove the wall, the graveyard and the grass, change nothing else. Asking Gemini for lighter colours as well made it pale pink and toy-like, so the colour is done in code. The user chose step 2 of four (gamma 0.8, warmth 0.5).

```
python ../romanian-buildings/work/clean_background.py gemini/temple_age1-3_nowall.jpg work/temple_0m_bg.png
python ../roman-buildings/tools/make_sprite.py work/temple_0m_bg.png for-gemini/temple_0m__original.tga work temple_0m 0 --shadow
python ../romanian-buildings/work/shadow_clean.py work/temple_0m_B.tga work/temple_0m_sh.png
python ../roman-buildings/tools/rescale_sprite.py work/temple_0m_sh.png work/temple_0m_s82.png 0.82 131 142
python ../roman-buildings/tools/brighten_sprite.py work/temple_0m_s82.png clean/temple_0m.png 0.8 0.5
python ../roman-buildings/tools/write_like.py clean/temple_0m.png for-gemini/temple_0m__original.tga nation_art/french/temple_0m.tga
python ../roman-buildings/tools/depth_mesh.py temple_0m
```

At full height it overflowed its footprint twice as much as the original (32.9 px against 15.7); at 0.82 it is 18.0. The depth mesh covers 88 % of the sprite.

## Small City, ages 1-3

The old picture (`northern-european-buildings/gemini/small-city_age1-3_noground.jpg`) edited in a new Gemini chat: "turn the chapel into an ordinary house like the others, in the same place and size; remove the smoke, hay stack, carts, straw and campfire; keep all the other buildings where they are". Asking to move the houses closer together first left too few buildings and a gap where the chapel was. Then the same steps as the Temple, with `gemini/small-city_age1-3_nochapel.jpg`, scale 0.81 around (262, 196) (overflow 57.3 down to 24.9, the original's 24.6) and the same colour step (0.8 0.5).

## Large City, age 3

The old picture (`northern-european-buildings/gemini/large-city_age3_noground.jpg`) edited in Gemini: "turn the big stone church at the back into ordinary tall half-timbered town houses; remove the campfire; keep the rest" (`gemini/large-city_age3_nochurch.jpg`). Then the Temple's steps to `work/city_0ma_lit.png` (scale 0.86 around (295, 268): overflow 36.2 down to 14.4, original 12.5), and:

```
python tools/short_shadow.py work/city_0ma_lit.png work/city_0ma_short.png 0.35 0.2
python tools/city_fix.py work/city_0ma_short.png clean/city_0ma.png
```

`make_sprite`'s fitted shadow shift (0.75, 0.4) threw long streaks across this tall, dense town; `short_shadow.py` redraws it at about half. `city_fix.py` drops a dark wedge in the square (Gemini's shadow, keyed as building) and paints the player-colour band the picture lost with the church. A restart from the game's original city came out with mixed slate and tile roofs and the Greek columns, outside the style of the other French buildings, and was dropped: every building of the set starts from our own pictures.

## Library, ages 1-3

The old picture, edited in Gemini: asked for longer wings and no bell turret, it only dropped the turret and the red band; asked for the band back, it returned the first image, turret included (`gemini/library_age1-3_band.jpg`, the one used). Same steps as the Temple; it filled the whole canvas width, so scale 0.78 around (203, 236): overflow 26.4 down to 3.7 (original 1.9). A move down only pushed the wing ends past the diamond's side corners. The open courtyard leaves the front corner empty, like the original's stairs court.

Another session's rebuild rewrote `nation_art.ini` on 2026-10-31 and dropped the Temple rule; check the `[french]` section after any pack or Romania rebuild.

## Barracks, ages 1-3

The old picture as it was, with every prop (`gemini/barracks_age1-3.jpg`; an edit that removed the campfire, benches and crates was wrong and dropped). `make_sprite` without `--shadow`, no rescale needed (overflow 12.1, original 14.1), `brighten_sprite.py` 0.7 0.7: the other French sprites average a brightness of 91-98, the shared barracks 62, this one 100.

## Market, ages 1-3

First the old picture re-imported lighter (`work/market_0m_oldpicture.png`). The user wanted it regenerated: Gemini, new chat, the old picture and "reimagine this market as a French medieval market hall ... keep all the goods and stalls" gave a bigger hall with a cross-gabled roof on posts (`gemini/market_age1-3_hall.jpg`). The user liked its shadow, so this one keeps Gemini's own shadow:

```
python ../romanian-buildings/work/clean_background.py gemini/market_age1-3_hall.jpg work/market2_bg.png
python ../roman-buildings/tools/make_sprite.py work/market2_bg.png for-gemini/market_0m__original.tga work market2 0
python ../roman-buildings/tools/gemini_shadow.py gemini/market_age1-3_hall.jpg work/market2_bg.png for-gemini/market_0m__original.tga work/market2_B.tga work/market2_sh.png
python ../roman-buildings/tools/brighten_sprite.py work/market2_sh.png work/market2_lit.png 0.7 0.7
```

`gemini_shadow.py` finds Gemini's shadow (darker magenta) in the raw picture and places it with make_sprite's own fit. That version (`work/market_0m_magenta.png`) had a white contour round every object and magenta spill under the hall: make_sprite.py repaints pinkish edge pixels as sand. The user dropped magenta for good, and Gemini put the same hall on white (`gemini/market_age1-3_hall_white.jpg`):

```
python ../roman-buildings/tools/make_sprite_white.py gemini/market_age1-3_hall_white.jpg for-gemini/market_0m__original.tga work/market3.png --keep-shadow
```

`--keep-shadow` (new) takes Gemini's soft grey shadow as alpha 145: neutral, smooth, not dark, joined to the white; the light rim between shadow and white goes to the background. A first run still showed light pixels and a ragged edge in the town picture (the user: "multi pixeli albi ... ciopartita si zimtata"): the ground showed through a 1-2 px gap between the shadow and the props (the outline is pulled in by a pixel, the shadow stopped at the old one), the grey floor under the hall, cut off from the white, stayed as building, and the mask's ragged edge zigzagged at game size. Now the gap is closed, enclosed big neutral grey areas count as shadow, and the mask is smoothed (Gaussian 4 px at picture size) before it is scaled. The red band and cloth were made player colour (saturated red areas of 12 px or more, grey from luminance), then `brighten_sprite.py` 0.7 0.7 (brightness 107) into `clean/market_0m.png`. No rescale (overflow 20.9, original 21.0).

The keyed outline still looked chopped up in the town picture, and the user asked for a proper background cutter. The installed version uses the BRIA RMBG model already in the local ComfyUI, and the user gave Gemini's picture twice, without and with its shadow (`gemini/market_age1-3_hall_white_noshadow.jpg`, `..._shadow.jpg`):

```
C:\AI\ComfyUI-Easy-Install\ComfyUI-Easy-Install\python_embeded\python.exe ../roman-buildings/tools/rmbg_matte.py gemini/market_age1-3_hall_white_noshadow.jpg work/market_matte.png
python ../roman-buildings/tools/make_sprite_white.py gemini/market_age1-3_hall_white_noshadow.jpg for-gemini/market_0m__original.tga work/market4.png --matte work/market_matte.png --shadow-pic gemini/market_age1-3_hall_white_shadow.jpg
```

then the player colour (as above) and `brighten_sprite.py` 0.7 0.7 into `clean/market_0m.png`. The matte follows posts, stall legs and laundry; the shadow is where the shaded picture is darker, smoothed. Overflow as the original's.

The shadow was then removed too (the user: "ti ai batut joc cu umbra aia"; the version with it is `work/market4_with_shadow.png`). The game draws a shadow at one fixed darkness, so Gemini's soft, fading shadow turns into one big hard slab whatever the cut. The Market, like every French building, has no shadow. A dithered version of Gemini's shadow (`roman-buildings/tools/dither_shadow.py`, `work/market_dither_*.png`) was shown too; the user chose no shadow.

From here on every French picture is on white, cut with `rmbg_matte.py` + `make_sprite_white.py --matte`; ask Gemini for the picture with and without its shadow when the shadow is wanted.

## Sizes changed by the user (2026-10-31)

- Temple: 15 % bigger and 10 px to the left (`rescale_sprite.py ... 1.15 131 142 -10 0`, from `work/temple_0m_before115.png`).
- Library: 10 % bigger (`rescale_sprite.py ... 1.10 203 236`, from `work/library_0m_before110.png`).
- Small City: 28 px to the right (`rescale_sprite.py ... 1.0 262 196 28 0`, from `work/village_0ma_before_shift.png`); it sat left of its plot.
- Temple again, asked +10 %: only +6 % fits (`rescale_sprite.py ... 1.06 121 142`, from `work/temple_0m_before110b.png`): the sprite canvas is 207 px high and the tower's tip and the front steps already reach both edges. A bigger church would need a bigger canvas, which `write_like.py` does not allow (same header as the original).

## Shadows (2026-10-31): none

The French sprites have no ground shadow made in code. Every generated one looked worse than none, and the user had them all removed; new French sprites get none either (`make_sprite.py` without `--shadow`). The one exception is the Market, which keeps the shadow Gemini drew (`gemini_shadow.py`). The history, kept for the record:

`make_sprite --shadow` treated every pixel as standing over the lowest pixel of its column, so the church tower threw a hard grey slab ("umbra asta mizerabila"), and a short version of it (`tools/short_shadow.py`) only hid the problem. Now every French sprite gets its shadow from its 3D shape, as the last step before `write_like.py`:

```
python ../roman-buildings/tools/cast_shadow.py cast <texture>.tga clean/<name>.png clean/<name>.png
```

It takes the depth mesh's height field and casts it with the game's light, to the lower left with the originals' length (see `roman-buildings/ENGINE_NOTES.md`), with smoothed edges. A first version cast up-left behind the buildings, four times longer, with zigzag edges; the user found it unnatural. The corrected one still looked worse to the user than no shadow, so all five were made shadowless (alpha 145 set to 0 in `clean/`, then `write_like.py`).

Check a building among the others with `python ../romanian-buildings/work/age_town.py 3 <out.png> --nation french`.
