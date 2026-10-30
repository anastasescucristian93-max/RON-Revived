# Nations

What each nation looks like now, what is done and what to improve next, in order. Update it after every change to a nation. General work (engine, interface, tools) stays in `TODO.md`; finished work for players goes in `CHANGES.md`.

The user plays a random match and improves the nation they get. Unless a line says otherwise, finished art is **not yet tested in game**.

**Goal (the user, 2026-10-26):** every nation has its own style in **every** age: 1-3, 4-5 and 6-8 (towers and forts also 4, 5-6, 7-8). Ages 1-3 are not more important than the others. A nation that has "its own" set but shares it with other nations still needs its own. Inside one set, a building can differ per age (`@<age>` rules) when the style makes no sense in an earlier age, like the American meeting house, which is shown in age 3 only.

## How the game shares art

- **Building ages:** the game has three building sets per style: ages 1-3, ages 4-5 and ages 6-8. Towers and forts have four: 1-3, 4, 5-6 and 7-8. Sources: `Data/building_graphics.xml`, `tribes\<nation>.xml` (`<BUILD_CONTINENT>`, `<UNIT_CONTINENT>`).
- **Ages 1-3, only four sets for 24 nations:**
  - Greco-Roman (`_0m`): Greeks, Romans, Spanish, British, Dutch, French, Germans, Russians, Americans, Romania (the 25th nation, `romania` mod).
  - Aztec / desert (`_0s`): Aztecs, Maya, Inca, Turks, Egyptians, Bantu, Nubians, Indians, Persians.
  - Chinese (`_0a`): Chinese, Japanese, Mongols, Koreans.
  - Iroquois (`_0i`): Iroquois, Lakota.
- **Ages 6-8:** most nations get the same generic modern buildings (`_6`).
- **Units:** six styles.
  - European: Americans, British, Dutch, Egyptians, French, Germans, Greeks, Romans, Russians, Spanish, Turks.
  - Arab: Bantu, Nubians.
  - American: Aztecs, Inca, Maya.
  - Asian: Chinese, Japanese, Koreans, Mongols.
  - Iroquois: Iroquois, Lakota.
  - Indian: Indians, Persians.
- **How a nation gets its own art:** `nation_art` (`camera-smooth/src/nation_art.c`, rules in `roman-buildings/nation_art.ini`) swaps building sprites per nation and age. It cannot yet swap unit textures. A unique unit can get other art in `unit_graphics.xml` alone. Aircraft can change model per nation through `<TRIBE_MASK>` in `unitrules.xml`.
- **Base pack, in order:** Small City, Large City, Temple, then Barracks, Market, Library. Houses need a test first: is one house swapped by `nation_art`?
- **Animated buildings:** some sprites have animated 3D addons drawn on top (`EXISTEVENT` in `building_graphics.xml`), which can float over new art. The user avoids them for now. In the `_0s` set: Granary, Smelter, Barracks (forge with smoke), Small City and Large City. In the `_0m` set (Americans and the Greco-Roman nations): Small City, Large City (forge, chimney smoke), Granary, Lumber Mill, Smelter, Barracks, Stable.

## Americans

- **Buildings now:** Greco-Roman (1-3, with our Temple in age 3), their own colonial style (4-5), generic modern (6-8).
- **Units:** European style. The Armored Car and the Fighter use the generic models (the Fighter is a Mustang).
- **Done:** Temple, age 3 only (`temple_0m.tga@3`), 2026-10-26 (`american-buildings`): a Puritan meeting house like Old Ship Church (1681). Ages 1-2 keep the Greek temple.
- **Decided:** the user does not want the northern European set for them (2026-10-26). They get their own art.
- **To improve:**
  1. Ages 1-2. A colonial style makes no sense there, so another idea is needed: before the colonists, the land's own peoples (Mississippian mound towns like Cahokia, Ancestral Puebloan cliff houses), to be agreed with the user.
  2. Age 3: the rest of the base pack in the early colonial style (1600s: Jamestown, Plymouth): Market, Library. Library prompt given 2026-10-26 (Harvard Old College, 1638, with the Cambridge Press), waiting for the image; reference `american-buildings/for-gemini/library_age1-3_original_magenta.png`. The Small City, Large City and Barracks have animated addons (forge, smoke).
  3. Ages 6-8, now the generic modern set: their own American look (for example, 1900s brick downtown, skyscrapers, Capitol-style Senate).
- **Gemini tips:**
  - A reference image on a green background made Gemini paint a green backdrop: put every reference on magenta.
  - A plain "meeting house" came out as a bare shed. Naming the real building (Old Ship Church, Hingham) and listing its parts (two storeys, gables, railed deck with belfry, porch) gave a rich one.
  - Gemini paints "bright red" as dark red. The player colour is picked by hand (`american-buildings/work/temple_team.py`).
- **Balance, to check (the user finds it overpowered, 2026-10-26):** every Barracks unit except the Scout gives +3 food, timber, metal and wealth while it is not garrisoned. That is `help.xml` TRIBE20; `rules.xml` `AMERICANS_BARRACKS_GATHER` says 2. It works from age 1: Slingers/Javelineers, Hoplites/Phalanx/Pikemen, Bowmen/Archers/Crossbowmen, later Musketeers, Marines, machine guns, anti-tank.
  - Still unknown: the real value (2 or 3), and whether it counts against the Commerce Cap (70 in age 1, up to 500).
  - First step: a test in game. Note the income rates, make 10 soldiers, note them again; repeat at the cap.
  - Options if it is too strong: +1 instead of +3; only from age 4, when the Americans really exist; only the first N units count; wealth only. Changed values only work online when every player has the mod.
- **Later:** maize fields (see "Farms per nation" in `TODO.md`). Balance: apart from the Barracks income, the units have only +6 % hit points, and the other bonuses come late.

## Aztecs

- **Buildings now:** their own set in ages 1-5 (Aztec / desert `_0s`, then `_3s`) and in 6-8 (`_6s`).
- **Units:** American style.
- **To improve:**
  1. They share every building with the Maya and Inca in all ages: each of the three needs its own style, per age (for example Tenochtitlan for the Aztecs).
  2. Bomber: He 111 -> B-25 (an aircraft mask change only).
- **Done:** Temple 1-3 (`aztec-buildings`), 2026-10-28: an early Templo Mayor, a stepped pyramid with a double stair and twin thatched shrines (Huitzilopochtli with the player-colour band and door cloth, Tlaloc in blue and white stripes), in place of the desert set's stone circle. Scale 0.88 (footprint_check), wear 1.0, depth mesh made.
- **Next:** Temple 4-8 (`temple_3s`, a Maya-looking pyramid shared with the Maya and Inca): the big stone Templo Mayor with twin stairs and shrines with carved roof crests. Then Small City 1-3 (chinampa houses of adobe and thatch). Exports of both temples in `aztec-buildings/for-gemini`.
- **Gemini tip:** Gemini would not turn the single stair into a double one in the first edit; it did after asking again. Gemini's picture was a screenshot: crop the black frame and the dark gradient at the bottom before make_sprite.
- **Later:** maize fields.

## Bantu

- **Buildings now:** Aztec (1-3), Ottoman (4-5). In 6-8 only the Small City, Large City, Library and Temple are their style's own (`_6t`).
- **Units:** Arab style. The BMP and T-80 are the generic models.
- **Done:** nothing installed. Fort 1-3 (`fort_0s`, also the Castle) is started as Great Zimbabwe (`bantu-buildings/`).
- **To improve:**
  1. Fort 1-3. The style of the first two Gemini images is good, but the camera is turned about 20 degrees, so the front corner sits left of the middle. Both images are in `bantu-buildings/source/` (`fort_v1_gemini.jpg`, `fort_v2_gemini.jpg`). Their dark purple drop shadow can be keyed out, so only the camera blocks them. Next: a new Gemini chat with only the original as reference.
  2. A shared African set with the Nubians, ages 1-5. Ages 1-3: Great Zimbabwe (dry-stone walls, a conical tower, round thatched huts). Ages 4-5: the Swahili coast (Kilwa, coral stone). Base pack first.
- **Later:** millet or sorghum fields. Balance: may be too strong (population limit 250, +1 city, cities 75 % cheaper, faster units).

## British

- **Buildings now:** northern European medieval (1-3, ours), northern European (4-5), generic modern (6-8).
- **Units:** European style. Their modern infantry and tanks are the German WWII models.
- **Done:** the full base pack for ages 1-3 (`northern-european-buildings`, files in `nation_art\northern_europe\`, shared with the French, Germans and Dutch). Done 2026-10-25.
- **To improve:**
  1. If wanted: Tower, Fort, Granary, Stable 1-3, for all four nations at once.
  2. 2026-10-30: the user finds this set ugly as seen on the French (see French). A rework would change it for all four nations.
- **Gemini tip:** Gemini insists on a paved floor under markets. What worked was a new chat with "the cobblestone floor is NOT part of the sprite, erase it".
- **Later:** balance, Longbowmen may be too strong (+2 range, cheaper).

## Chinese

- **Buildings now:** their own Chinese set, all ages.
- **Units:** Asian style. The BMP and T-80 are the generic models.
- **To improve:**
  1. They share ages 1-8 with the Japanese and Mongols, and ages 1-3 with the Koreans. Keep the current art for the Chinese and give the others their own.
  2. A new strategic bomber, the H-6 (new art, shared with the Egyptians' Tu-16).
- **Later:** rice fields. Balance: may be too strong (citizens instantly, Large Cities from the start).

## Dutch

- Same buildings and status as the British (shared northern European set, ages 1-3 done 2026-10-25).
- **Units:** European style, with German WWII models in the modern ages.
- **Done:**
  - Their own Data Center (2026-10-26, not yet tested in game): Dutch modernism (brick hall, glass curtain walls, glass stepped gables, turning cooling-tower fans).
  - Tower, ages 7-8 (`tower_6`, 2026-10-26, `dutch-buildings`): a 1930s concrete observation tower on a brick base with a riveted steel cupola, like the Dutch fort. Came right in one Gemini image with the game's tower as camera reference and the Dutch fort as style reference. Fits the footprint at scale 1.
  - Temple, ages 4-8 (`temple_3n`, 2026-10-26, `dutch-buildings`): a Golden Age Amsterdam church, brick and white stone, stepped gable with volutes, roof dormers, a Westertoren-style tower with the blue and gold crown. Scaled to 0.92 and moved (-12, -4) to sit on the 4x4 footprint. Player colour: the band under the eave and the cloth over the door (`work/temple_team.py`).
- **Gemini tips:** a 3x3 grid of variants comes out small; asking for one of them alone makes it hazy (an enlargement). A new chat with the camera reference + a screenshot as style reference keeps the shape but copies the haze; at game size (half the image) it disappears after sharpening in code. Gemini keeps adding a paved plate under buildings: say no plate, pavement, kerb or ground shadow.
- **To improve:**
  1. Fort 7-8 is done (2026-10-26, `dutch-buildings/`, rule `fort_6.tga` for both ages): a 1930s casemate fort after Kornwerderzand, a tall concrete keep with a cupola, three cupola turrets on the outer wall, a brick base. Lessons: with the original fort as the only shape reference Gemini just re-textured it (low poly); without it, it lost the tall keep. What worked: "keep the composition and silhouette of image 1" listed piece by piece. The user did not want grass roofs (the real Dutch forts had them) and wants the turrets on one level to match. A separate age 8 was dropped: "modernised" versions (Goalkeeper, radar panels, stealth turrets) looked like sci-fi super weapons, and realistic modern bases (T-walls and containers, a Cold War bunker, a walled barracks) looked like a car park or a prison. The user agreed with the game, which keeps one fort for both ages.
  2. Ages 4-5, Golden Age (1600s), the rest of the static buildings: Market (Hendrick de Keyser's Exchange, 1611, or a Waag), Dock (a VOC shipyard with an East Indiaman), Library (Leiden University Library). The Small City, Large City, Senate, Barracks, University, Granary, Lumber Mill, Smelter and Stable of the `_3n` set are animated (`EXISTEVENT`); the Senate would be the Paleis op de Dam later.
  3. Own buildings in ages 1-3 (now the shared northern European set; proposed brick Gothic) and 6-8 (Amsterdam School / modernism).
- **Gemini tips (temple):** Gemini cannot draw a roof made of several pieces. Westerkerk with transepts and side gables came out with a roofless courtyard, then a pit between two roofs, then a trench; corrections in the same chat repeated the image. What worked: a new chat asking for ONE short hall with ONE two-sided roof and one straight ridge ("no side gables, no smaller roofs, no courtyard"), without the name Westerkerk, then in the same chat adding detail only on the existing surfaces (dormers, stepped gables, pilasters, cornice).

## Egyptians

- **Buildings now:** ours in ages 1-3 for part of the set, the rest Aztec. Ottoman in 4-5.
- **Units:** European style, so every foot soldier (hoplites, archers, pikemen, musketeers, riflemen, infantry) looks Greek or European. Only their cavalry is their own. Their tanks, BMP and MiG are the generic models.
- **Done:**
  - 2026-10-24: Small City 1-3, Large City 3, Temple, Barracks, Market and Library 1-3.
  - 2026-10-25: Senate 1-3, the pharaoh's palace.
  - All in `egyptian-buildings`.
- **To improve:**
  1. Test in game. Check that the Barracks' animated forge (`FORGE_0M_ADDON`, with smoke) does not float over the new art.
  2. Remaining ages 1-3, all without animations: Lumber Mill (`sawmill_0s`), University (`university_0s`), Siege Factory (`forge_0s`), Dock, Tower, Fort. The Granary and Smelter have animations; the user does not want them for now.
  3. Houses (after the house test).
  4. Ages 4-5 base pack (now Ottoman); a Mamluk Cairo is one idea.
  5. Units: their own foot soldiers. This needs `nation_art` to swap unit textures (engine work).
  6. New aircraft art: Il-28 bomber, Tu-16 strategic bomber, Mi-8 helicopter.
- **Gemini tips:** use the finished Library image (`gemini/library_age1-3.jpg`) as the style reference, so the set stays one family. Gemini copies the style image too closely at first: ask again for the details that make the building what it is. When it puts a stray object somewhere, ask it in the same chat to remove only that object; it keeps everything else.

## French

- Same buildings and status as the British (shared northern European set, ages 1-3 done 2026-10-25).
- **Units:** European style, with German WWII models in the modern ages. Jet fighter: Mirage.
- **To improve:**
  1. 2026-10-30, the user, from a screenshot of Paris in ages 1-3: the French buildings look ugly. These are our shared northern European set (Small City, houses, chapel, well). What shows in the screenshot: dull, muddy brown-olive colours, blurry mossy thatch, and the player-colour bands look like a straight red line pasted across the walls. Options: give the French their own ages 1-3 set (Romanesque / early Gothic stone, slate roofs, Île-de-France), or rework the shared set for all four nations. Ask the user which.
     - 2026-10-31, from the town compositions (`romanian-buildings/work/age_town.py <age> <out> --nation french`): the user wants them regenerated. In ages 1-3 only six are ours (Small City, Large City, Temple, Barracks, Market, Library); the rest are the game's Greco-Roman ones (columned Senate and University, a thatched timber Fort), which clash with ours. Ages 4-5 are the game's northern European set (half-timber, English Gothic, a Big Ben senate: British, not French); ages 6-8 the generic modern set.
     - Why ours look bad: (a) the user: the Small City is "chopped up". The shadows are ragged blocks: our own, not Gemini's. On 2026-10-25 `make_sprite.py` always projected every pixel onto the ground by its height in its column (`project_shadow`); with houses, carts and fences side by side that gives long hard rectangles crossing the plot. Gemini's purple shadows were keyed out, but their darkest parts passed the magenta key as building pixels and stay as grey stains (round the well). Thin things (fence, cart, stairs) lost pixels where the downscaled alpha fell under 128. The game's alpha is one bit (0 / 255 / 248 team / 145 shadow), so the new art needs a clean outline at game size, no Gemini shadow, and at most a short shadow. (b) The `_worn` step (weather.py, banned since 2026-10-28) muddied the colours; the Gemini images (`northern-european-buildings/gemini/`) are warmer and cleaner than what is installed. (c) The Library sits back on its 5x5 plot: the front corner is empty and the back sticks out (`footprint_check.py library_0m.tga`). New art must fill the plot to its front corner, as the original's stairs do.
     - Temple: the user wants the new church without the churchyard wall and graves.
     - Large City: regenerate it without the church (the Temple is the church).
     - Plan (2026-10-31): the French get their own ages 1-3 set; the British, Germans and Dutch keep the shared one for now. Order, one at a time: Temple, Small City, Large City, Library, Market, Barracks; then the game's Greco-Roman ones that clash (Senate, University, Fort, Tower, then the rest). Each from a short prompt on the game's original sprite; checked in `age_town.py 3 --nation french` before it counts as done.
     - Colour: the user wants the new set a little lighter, in the game's bright warm tone (pale limestone like the original Greco-Roman sprites), not dark and mossy.

## Germans

- Same buildings and status as the British (shared northern European set, ages 1-3 done 2026-10-25).
- **Units:** European style.
- **To improve:**
  1. Bomber: B-25 -> He 111 (`bomber_german`), an aircraft mask change only.

## Greeks

- **Buildings now:** their own Greco-Roman set (1-3), Mediterranean (4-5), Mediterranean modern for the cities (6-8).
- **Units:** European style. Their modern infantry and tanks are the German WWII models.
- **To improve:** they share their buildings with the Spanish (and the Romans until those are done) in every age. They need their own style per age, for example Classical Athens, Byzantine, then modern Greek.

## Inca

- **Buildings now:** Aztec set, all ages (shared with the Aztecs and Maya).
- **Units:** American style.
- **To improve:**
  1. They share every building with the Aztecs and Maya in all ages: they need their own Andean style per age (Cusco stonework, Machu Picchu terraces).
  2. Bomber: He 111 -> B-25.
- **Later:** maize fields. Balance: Inti Clubmen may be too strong.

## Indians

- **Buildings now:** Aztec (1-3), their own Indian style (4-5), Ottoman modern for the cities (6-8).
- **Units:** Indian style.
- **To improve:**
  1. Ages 1-3 base pack (now Aztec).
  2. Aircraft: Fighter Yak -> Spitfire, Jet Fighter F-16 -> MiG (mask changes only).
- **Later:** rice fields.

## Iroquois

- **Buildings now:** their own set (1-5), generic modern (6-8).
- **Units:** Iroquois style.
- **To improve:** they share every building with the Lakota in ages 1-5, and get the generic modern buildings in 6-8. They need their own style in 6-8, and the Lakota their own in every age.
- **Later:** maize fields.

## Japanese

- **Buildings now:** Chinese (1-5).
- **Units:** Asian style.
- **To improve:**
  1. Base pack, ages 1-3 then 4-5.
  2. Strategic bomber: Backfire -> B-52 (mask change only).
- **Later:** rice fields. Balance: Samurai may be too strong.

## Koreans

- **Buildings now:** Chinese (1-3), their own Korean style (4-8).
- **Units:** Asian style. Their citizens and scouts are the generic models.
- **To improve:**
  1. Base pack, ages 1-3.
  2. Strategic bomber: Backfire -> B-52.
- **Later:** rice fields.

## Lakota

- **Buildings now:** Iroquois (1-5).
- **Units:** Iroquois style.
- **To improve:**
  1. Tipis, ages 1-5, base pack first.
- **Later:** maize fields.

## Maya

- **Buildings now:** Aztec set, all ages.
- **Units:** American style.
- **To improve:**
  1. They share every building with the Aztecs and Inca in all ages: they need their own style per age (Tikal, Chichen Itza).
  2. Bomber: He 111 -> B-25.
- **Later:** maize fields.

## Mongols

- **Buildings now:** Chinese (1-5), with our Small City in ages 1-3.
- **Units:** Asian style. The BMP and T-80 are the generic models.
- **Done:** Small City 1-3 (`mongol-buildings`), 2026-10-25. Fixed 2026-10-26: it sat too far back and left on its ground (back yurts behind the outline); scaled 0.9 around the canvas bottom and moved (12, 26) by `work/settle_village.py`, the unmoved clean copy is `work/village_0a_unsettled.tga`. To check in game.
- **To improve:**
  1. Temple, Barracks, Market and Library 1-3 (`_0a`).
  2. Large City 3 (`city_0a`) is parked. A yurt camp looked too much like the Small City, and a walled Karakorum came out as an empty box with a Chinese roof. Untried idea: a dense town with no wall, around a giant felt palace-tent.
  3. Ages 4-5 in yurts.
- **Gemini tip:** Gemini makes yurts look like toys at first. Ask for patched, soot-stained, sagging felt.

## Nubians

- **Buildings now:** Aztec (1-3), Ottoman (4-5), with our Temple in ages 1-3.
- **Units:** Arab style. The BMP and T-80 are the generic models.
- **Done:** Temple 1-3, the pyramids of Meroë (`nubian-buildings`), 2026-10-25. Fixed 2026-10-26: the 3D render had cut off the front corner of the right pyramid (`render_iso.py` did not centre the model on screen); re-rendered whole (`work/iso_az-45_v2.png`), toned like the first version (`work/match_old_tone.py`), scale 0.88 as before.
- **To improve:**
  1. Small City 1-3: mud-brick houses with domes, painted gates.
  2. The rest of the shared African set with the Bantu.
- **Gemini tip:** Gemini first draws Egyptian (Giza) pyramids. "Height twice the base, 70 degrees, like spearheads" gave the Nubian shape, but from a low front camera, and no prompt would turn the camera. The front view was used as it is; it reads fine at sprite size.
- **Later:** millet or sorghum fields.

## Persians

- **Buildings now:** Aztec (1-3, with our Temple), Indian (4-5), Ottoman modern for the cities (6-8).
- **Units:** Indian style.
- **Done:** Temple 1-3 and Senate 1-2, 2026-10-26.
- **To improve:**
  1. Ages 1-3, now Aztec (`_0s`), split by period: Achaemenid (Persepolis) fits ages 1-2, Sasanian (iwan arches like Taq Kasra, chahar taq fire temples) fits ages 2-3. Temple 1-3 done 2026-10-26 as a Zoroastrian fire temple (chahar taq), which fits all three ages (`persian-buildings/`). Senate ages 1-2 done 2026-10-26 as the Apadana of Persepolis (`senate_0s.tga@1`, `@2`). Next: a Sasanian palace for the Senate in age 3 (Taq Kasra, the great brick iwan of Ctesiphon). Without animations here: Senate, University, Lumber Mill, Siege Factory, Dock, Library, Market, Temple, Tower, Fort.
  2. Ages 4-5, now Indian (Mughal): Safavid Isfahan (blue-tiled domes, iwans, the Naqsh-e Jahan square).
  3. Ages 6-8, now Ottoman modern (`_6t`) and generic: Pahlavi-era and modern Tehran (Azadi Tower).
  4. Fighter: Yak -> Spitfire (mask change only). New art: F-14 Tomcat.
- **Gemini tips:** with no style image of the nation yet, the camera reference alone (the game's sprite on magenta) and "realistic, in the game's style" gave a good first image. The smoke over the fire came out pinkish, but after the key it is a small grey wisp, which reads fine. Persepolis: asking for glazed brick made Gemini paint the whole towers like Babylon's Ishtar Gate. What worked: stone, black polished stone door frames, stepped battlements, and only one narrow glazed band with the Susa archers. Ask for a roof outright, or Gemini leaves a hypostyle hall open like a ruin.
- **Later:** balance, the Immortals may be too weak.

## Romania

- **New nation** (`romania`, 2026-10-26, not yet tested in game): tribe 24, key `romania` (for `nation_art.ini` sections).
- **Buildings now:** the game's Northern European style (`BUILD_CONTINENT 1 NEuro`): Greco-Roman `_0m` in ages 1-3, Northern European `_3n` in 4-5, generic modern in 6-8. None of our art.
- **Units:** European style; in every line the model the Russians have, without the Russian uniques. 12 units of its own (stats); the first with a model of its own is the IAR-80 fighter (2026-10-28, Tripo model by the user, not yet tested in game). Next, in order of difficulty: TACAM T-60 (Armored Car line, user generating it), R-2, TR-85M1 Bizonul, LAROM; infantry and cavalry (Dorobanti, Vanatori de Munte, Falxmen, Roxolani Lancers) last, they need the biped skeleton. Gemini paints German crosses when asked for Michael's Cross: ask for no insignia, paint them in code (`tools/make_plane.py`).
- **To improve** (proposal, to agree with the user; one building at a time):
  1. Ages 1-3, Dacian: Sarmizegetusa Regia (the round and rectangular sanctuaries with andesite columns, the stone walls of the citadels with the murus dacicus) for the Temple and Fort. The Small City of ages 1-3 is an old Romanian village like the Village Museum's (the user, 2026-10-27, after a Dacian hut village): limewashed clay houses with very tall, steep thatched roofs, a log house, wattle fences capped with straw; the player colour is the painted band (brau) at the foot of the white walls.
  - DONE 2026-10-27: Temple 1-3 (`temple_0m`), the Great Round Sanctuary (`romanian-buildings/gemini/temple_age1-3_v2.jpg`, scaled 0.92 and moved (-3, -12) to fit its footprint).
  - DONE 2026-10-27: Small City 1-3 (`village_0ma`), the Village Museum houses (`romanian-buildings/gemini/small-city_age1-3_v1.jpg`). The fence behind the log house ended in the air: `romanian-buildings/work/fence_fix.py` continues it along its slope. Purple-tinted pixels in the wattle (magenta through the gaps) neutralised after make_sprite (cleaning the source instead ate the bench legs). Seen in game 2026-10-27: it sat left of its ground; now scaled 0.9 and moved (19, -12), spanning the ground like the original. The front bench lost its legs at the bottom of the canvas: the fire pit and benches were lifted 40 px in the picture (`gemini/small-city_age1-3_v1_edited.png`, fence fix included). The game's campfire smoulder stays where the old pit was, about 20 px lower. Smoke and the campfire's smoulder come out at sensible places (back roof, granary roof, fire pit).
  - No animated cloths: Romania builds in the NEuro style, whose Small City 1-3 carries only an invisible placeholder addon (`FORGE_0M_ADDON`; the cloths are the Med style's). To check in game: where the chimney smoke and the smouldering campfire come out on the new art.
  - Gemini: asked for a red band on the eaves, it paints a thick flat red ring sticking out around a conical roof; the user accepted it.
  - Towers and forts in every age (the user, 2026-10-27), one at a time, in this order. Slots (NEuro): fort `fort_0m` 1-3, `fort_3n` 4, `fort_4` 5-6, `fort_6` 7-8 (`@7` / `@8` to split); tower `tower_0m` 1-3, `tower_3n` 4, `tower_4` 5-6, `tower_6` 7-8.
    - DONE 2026-10-27: Fort 1-3 (`fort_0m`), a Dacian hill fortress like Costesti-Cetatuie (`romanian-buildings/gemini/fort_age1-3_v2.jpg`). Its cast shadow on the magenta was removed in code (`romanian-buildings/work/clean_background.py`: crops Gemini's black frame and flattens the darker magenta connected to the edge); asking Gemini to remove it made the picture blurred (`fort_age1-3_v3_blurred.jpg`). Footprint scale 0.97, so not rescaled. History: The first try (dressed limestone, square plan, corner towers) came out a clean, regular medieval castle; the user: too clean and modern for the first ages. Second prompt: an irregular ring of rough stone with a log palisade on top, a squat tower-house, thatched huts, a log gate. It came out cartoony (ink outlines) and lit from the wrong side; fixed in the same chat by naming the game's light (upper right) and asking for no line art. Gemini then added a cast shadow on the magenta. Its pictures here come with a black frame and rounded corners (that is how it generates them, not a screenshot): crop them at import.
    - DONE 2026-10-27: Tower 1-3 (`tower_0m`), a Dacian watchtower, rough limestone shaft with beam ends, an overhanging log storey, a steep thatched roof (`romanian-buildings/gemini/tower_age1-3_v1.jpg`; the fort picture was the style reference). The user took the first picture as it was.
    - Ground shadows (2026-10-27): the user wants them after all, in the game's direction. Fort and tower 1-3 were remade with `make_sprite.py --shadow` (fitted to the original's shadow), then `romanian-buildings/work/shadow_clean.py` drops the thin streaks a roof corner projects. The Temple and Small City 1-3 still have none.
    - DONE 2026-10-27: Fort 4 (`fort_3n`), Suceava Citadel (Stephen the Great) (`romanian-buildings/gemini/fort_age4_v2.jpg`, the user: "acum e bine, nu mai este perfect curat"). Scaled 0.94 on its footprint. `clean_background.py` now keeps dark doorways (Gemini's shadow and the black frame had leaked into the gate) and drops background specks (one made make_sprite fit the fort too high). History: The first picture was a neat, symmetrical box with flat triangular buttresses and an earth moat with a bridge (a ground patch); the user: "a iesit prost", "sa nu le mai faca perfecte". Second prompt (new chat): irregular and asymmetrical, rough patched stone, stepped buttresses built into the masonry, no moat or bridge.
    - DONE 2026-10-27: Tower 4 (`tower_3n`), Chindia Tower, Targoviste (`romanian-buildings/gemini/tower_age4_v2.jpg`). The first picture was good, but the band was a blurred marker line over the arches and a cloth hung from the gallery; fixed in the same chat (no cloth, a band painted on the masonry below the gallery, crisp edges, texture through the paint). That wording is the one to use for bands.
    - DONE 2026-10-27: Fort 5-6 (`fort_4`), an 18th-century Habsburg fortress like Alba Carolina (`romanian-buildings/gemini/fort_age5-6_v3.jpg`, a third, fresh chat). Scaled 0.95. Lessons: the baroque citadel gate came out as a Roman triumphal arch (the user: "nu are sens"), so ask for a plain arched military gate; cannons must be described one by one (one big gun on the keep, one per corner bastion, muzzle outward, "no cannon points inward") or Gemini turns them inward; a "painted" band came out dull brick red that the pipeline cannot read, so ask for bright, pure, saturated red (#E00000). Corrections in the same chat stopped working after two rounds; starting again with everything in one prompt worked at once.
    - DONE 2026-10-27: Modern tower 7-8 (`tower_6`, `romanian-buildings/gemini/tower_age7-8_v1.jpg`, first try, the user: "perfect"), a concrete watch and gun tower of the FNB line 1939-1944, Resita steel cupola, Romanian WWII three-colour camouflage.
    - DONE 2026-10-27: Modern fort 7-8 (`fort_6`, `romanian-buildings/gemini/fort_age7-8_v2.jpg`), the same FNB line, with the modern tower as the style reference so they match. The first picture's left corner was a cupola on a thin pillar over a separate gun pit; fixed in the same chat ("a casemate like the right one, mirrored"). One look for ages 7 and 8; an age-8 version (`fort_6.tga@8`) could follow.
    - All towers and forts of every age are done (2026-10-27), none yet seen in game.
  - DONE 2026-10-27: Large City 1-3 (`city_0ma`), an old Romanian market town in the Small City's style (`romanian-buildings/gemini/large-city_age1-3_v3.jpg`): inn, boyar's house with porch, white thatched houses with the band, a log house, a well sweep. Wear 1.5 instead of 1.0 (the user chose it from 1.0 / 1.5 / 2.0: "un pic mai uzate"). Lessons: the user did not want a church inside the town; given the original as image 1, Gemini copied its Mediterranean houses next to ours (mixed styles), so image 1 is now a plain grey silhouette (`romanian-buildings/work/silhouette.py`) for size and angle only; gate and fences removed (they belong to the Small City). To check in game: the animated addon of `city_0ma` (smoke, cloths) over the new art.
    - DONE 2026-10-27: Tower 5-6 (`tower_4`, `romanian-buildings/gemini/tower_age5-6_v3.jpg`, scaled 0.73: Gemini made it wide), a Romanian Army artillery tower of 1877-1890 (War of Independence, the Bucharest forts): stone and brick base, riveted iron bands and armour, one Krupp-style gun behind an iron shield, an iron observation cupola. The user wants it military and industrial, with metal ("ar trebui sa includa si metal ... e epoca industriala", "sa aiba aspect militar"). Dropped: an Oltenian cula (white plaster, arched loggia; the user: "nu are sens deloc", a house, not a tower, and lit from the left), the Sighisoara Clock Tower (tall and civil, does not fit the squat gun tower slot) and a Schumann armoured cupola of the FNB line (came out as a generic bunker dome, lit from the wrong side; the user: "cam ciudat, se potriveste cu designul romaniei?"). Tower 5-6: the Sighisoara Clock Tower.
    - Fort 7: the FNB line (Focsani-Namoloasa-Braila), WWI concrete. Fort 8: a modern Romanian army base. Tower 7-8: a concrete observation tower of the same line.
  2. Ages 4-5, Wallachian and Moldavian: Brancovenesc style (Hurezi Monastery, Mogosoaia Palace) for the Senate and Library, a painted Moldavian monastery (Voronet) for the Temple, Bran or Corvin Castle for the Fort, a Saxon fortified church (Biertan) for the Tower.
  3. Ages 6-8, Bucharest: the Romanian Athenaeum (University or Temple), the CEC Palace (Market), the Arcul de Triumf, the Palace of Parliament (Senate).
     - Cities 6-8 (the user, 2026-10-28), split per age (`village_6.tga@6`, `@7`, `@8`; same for `city_6`): age 6 Neo-Romanian (Ion Mincu: foisor loggias, arched porches on carved stone columns, glazed tile friezes, deep eaves on carved brackets) and Bucharest Art Nouveau / Belle Epoque; age 7 grim communist prefab blocks (the user: "blocurile sinistre comuniste din prefabricate"); age 8 not decided yet. Major City `metro_6` is one texture for all ages. Order: Small City 6, Small City 7, Large City 6, Large City 7, then age 8.
     - DONE 2026-10-28: Small City 6 (`village_6.tga@6` -> `village_6_age6.tga`, `romanian-buildings/gemini/small-city_age6_v3.jpg`, third round in the same chat). Footprint scale 1.0. Lessons: given our Large City 1-3 as the render-style image, Gemini copied its peasant houses (white cubes, a thick red plinth) for the small houses; naming each town house (loggia, Art Nouveau facade, porch, shop with windows) fixed it. Asked for one band of equal width on all, it widened it on some buildings only; the user took the third round as it was. make_sprite took only the upper half of the bands: `team_bands.py` completes them. The cast-iron lamps are too thin to survive the keying and are gone; the bench stays.
     - DONE 2026-10-28: Small City 7 (`village_6.tga@7` -> `village_6_age7.tga`, `romanian-buildings/gemini/small-city_age7_v2.jpg`, second round in the same chat; the first was `small-city_age7_v1.jpg`), a corner of a communist prefab estate. Rescaled 0.86 around (267, 250) and moved (30, 28): the picture sat high and to the left on its diamond; 0.9 with (32, 36) cut the front corner at the canvas bottom. Fixes after make_sprite: `team_bands.py` also turned the rust on the garages and the boiler house's brick into player colour, put back from the _A sprite; antenna remnants and a stray line dropped (they made footprint_check ask for 0.68); the fitted shadow of the tall blocks was long, trimmed to the original's reach (13 px) and to pieces that touch the building. Lessons: bare trees (thin twigs on magenta key badly) and a blank box were fixed in one correction round; the band on every building took that round too.
     - DONE 2026-10-28: Large City 6 (`city_6.tga@6` -> `city_6_age6.tga`, `romanian-buildings/gemini/large-city_age6_v1.jpg`, first try, Small City 6 as the style image): a Neo-Romanian town hall with arcades, glazed tile friezes and a turret, an Art Nouveau corner building with a domed turret, French eclectic houses with shops below. Scale 1.0, no move (it sits a little right of its diamond, within the original's overflow).
     - DONE 2026-10-28: Large City 7 (`city_6.tga@7` -> `city_6_age7.tga`, `romanian-buildings/gemini/large-city_age7_v2.jpg`, second round; first `large-city_age7_v1.jpg`), Small City 7 as the style image: a ten-storey prefab tower, L-shaped blocks over a row of shops, a kiosk, garages. Scale 1.0, no move. The correction round added the band to the tower and the front wing and removed the courtyard's asphalt and a patch of grass.
     - DONE 2026-10-28: Major City 7 (`metro_6.tga@7` -> `metro_6_age7.tga`, `romanian-buildings/gemini/major-city_age7_v1.jpg`). It came from the user's own short prompt: the original (image 1), the user's photos of Romanian and New Belgrade blocks (image 2, `for-gemini/communist_blocks_photos.jpg`) and "reimagine image 1 as a cluster of Romanian communist apartment blocks exactly like the photos in image 2; keep the same layout". My longer prompts failed: a second style image led to nine copies of the style blocks, and a list of elements led to a flat, empty plaza. The roads ended in a straight cut at the edge. Asked to fix that, Gemini made a collapse with rubble, so `work/road_ends.py` continues both roads past the cut as broken asphalt that thins out. The player colour sits on the rust-red balcony panels (`work/metro7_team.py`); the original has only 44 player-colour pixels. Scale 1.0, wear 1.0.
     - 2026-10-28: the player-colour bands of Small City 7 and Large City 7 were too heavy next to the Major City (the user). `work/thin_bands.py` keeps only the lowest 2 rows of each band as player colour and turns the rest of the parapet into concrete, a little lighter than the walls. The user chose 2 rows over 4 (`work/thin_bands_compare.png`). The thick-band clean copies are kept in `work/*_age7_thickbands_clean.png`; `clean/` now holds the thin ones.
     - Fixes the same day. Wear: weathering the thin version again changed the wear around the parapets, because the old band pixels became building and grew grime streaks; the user saw it at once ("mi-ai mai tras un efect de uzura"). Now the old worn sprite is kept and only the band pixels are swapped (`work/*_thin2_keepwear.png`). Black specks: Gemini's dark contour around buildings survived make_sprite as near-black outline pixels and spikes; `tools/clean_outline.py` removes the spikes and recolours the rest from the wall next to them. Run it on every new sprite.
     - 2026-10-28, later: all three communist cities were redone from the user's own versions on a white background (`gemini/*-city_age7_white.jpg`). The user did not want magenta any more, nor my wear filter. `roman-buildings/tools/make_sprite_white.py` finds the background by its shape (it flood-fills from the border), pulls the outline in by one pixel and leaves the colours as they are: no colour matching, no wear. Small City placed at 0.86 around (267, 250), moved (30, 10); with (30, 28) its front was cut at the canvas bottom. `work/finish_communist.py` drops pale antenna tips and turns the red bands into the thin line, catching dark red by ratio (the shop-row band is (84, 6, 4)). Major City: `work/road_ends.py --white`, `work/metro7_team.py` keeps panels of 40 px or more. The earlier clean copies are in `work/*_clean_before_white.*`.
     - To check in game: `city_6` (ages 6-8) carries an invisible placeholder addon (`FORGE_0M_ADDON`) and 4 chimney smoke nodes, `village_6` an animated addon (`village_6n_ADDON`, two anims) and 4 smoke nodes, all placed for the original houses: smoke may rise from the flat roofs of the blocks at odd places, and the addon may draw over the new art.
     - FIXED 2026-10-29 (the user, from game screenshots): Small City 7 and Large City 7 had heavy grey cast shadows (gone), a 2-row player-colour line (now 4: `romanian-buildings/work/fix_city7.py`), and looked hazy (contrast std 37, sharpness 25-38 against about 48 on the game's cities; `roman-buildings/tools/crisp_sprite.py`). Small City 7 sat far back on its plot (the tall block's base up to 50 px off the footprint diamond): base corners read by hand, scaled 0.80 around the diamond's front corner and shifted (-17, 1) (0.76 fits exactly; the user asked for 5% more). The old sprites are in `work/*_before_fix.tga`. Lesson: footprint_check's overflow only looks at the front edges; a back row standing behind the diamond passes it.
     - REDONE 2026-10-29 (the user: the 0.80 Small City 7 came out smaller and pixelated, Large City 7 hazy and offset): the plot paved in game is about 1.2x footprint_check's diamond (calibrated by template-matching the sprite in the user's screenshot at 1.5x zoom and laying the diamond over the red plot border); the 0.80 fit used the too-small diamond. `fix_city7.py` now fits to the 1.2x plot and resamples smoothly (it took the nearest pixel: jagged). Small City 7: 0.94, from the original, no extra sharpening. Large City 7: not moved (its content already reaches the canvas's right edge; the plot's right and front corners lie outside the 570x431 canvas, so no sprite of that size can cover them), contrast and sharpness raised a little more. R-2: in the rules it is the Light Tank of the Industrial Age and upgrades to the TR-77 in the Modern Age, so a Modern-Age game never shows it.
     - FIXED 2026-10-29: the modern tower (`tower_6`) looked cut at the top in front of the wheat: its overhanging top had no depth mesh; `depth_mesh.py` now covers overhangs with camera-facing cards. R-2: the track on the side away from the light was much darker in game; the track blocks' normals are bent 45 degrees up (`build_tank_bh3.py`).
     - DONE 2026-10-29: Senate 7-8 (`senate_3n.tga@7`/`@8` -> `senate_3n_age7.tga`, `romanian-buildings/gemini/senate_age7-8_v2.jpg`, second round in the same chat): the former Communist Party Central Committee building (Revolution Square, the Senate 1990-2005), grey Stalinist block, square columns, the balcony; band made continuous player colour in code; scaled 0.72 to its plot (fix_city7.py). Collage of every age-7 building: `romanian-buildings/work/age_collage.py 7` -> `work/romania_age7.png`.
     - DONE 2026-10-29: Temple 7-8 (`temple_3n.tga@7`/`@8` -> `temple_3n_age7.tga`, `gemini/church_age7-8_v2.jpg`): an old sooty Orthodox church, two towers, arched porch, icon over the door; the icon kept out of the player colour (only band-shaped runs of up to 9 px); scaled 0.80. Market 7-8 (`market_6.tga@7`/`@8` -> `market_6_age7.tga`, `gemini/market_age7-8_v1.jpg`): a 1980s communist market hall with the Alimentara shop; Gemini's pale floor removed; scaled 0.92. Idea from the user for the Library 7-8: the Politehnica building or Romexpo.
     - DONE 2026-10-29: Library 7-8 (`library_6n.tga@7`/`@8` -> `library_6n_age7.tga`, `gemini/library_age7-8_v3.jpg`): the National Library of Romania (begun 1986), full-height square columns; Gemini left an open courtyard in the roof, filled in code (inpaint + grain, `work/library_age7_closed.png`); scaled 0.85, sits a little back on its plot (canvas bottom; to recentre if the larger-canvas test with fort_6 works). Still the game's in age 7, out of style: University (observatory dome), Factory (brick hall with a glass roof), Auto Plant (Western garage), Lumber Mill, Dock.
     - DONE 2026-10-29: Factory 7-8 (`forge_6.tga@7`/`@8` -> `forge_6_age7.tga`, `gemini/factory_age7-8_v1.jpg`): the game's factory with only the walls changed to grey concrete panels (the user: "doar zidurile"); Gemini kept the shape, so the player colour was taken from the original sprite's own places (alpha 248 where the new one is solid). Same way next: Auto Plant (`stable_6`). Collages from now on in the winter version too (the user).
     - DONE 2026-10-29: Auto Plant 7 (`stable_6.tga@7` -> `stable_6_age7.tga`, `gemini/autoplant_age7-8_v3.jpg`, third round): communist concrete, asbestos-cement roof, warm light kept; age 8 keeps the shared modern plant of [all]. Band detection now takes only true red (r > 1.5 g and b): the warm lit windows had turned into player colour. Barracks 7-8 (`barracks_6.tga@7`/`@8`, `gemini/barracks_age7-8_v3.jpg`): the rounded huts kept (the user), in concrete, tent/fire/crates redrawn sharp in a second round.
     - DONE 2026-10-29: University 7-8 (`university_6.tga@7`/`@8`, `gemini/university_age7-8_v1.jpg`, first try): the game's own in grey concrete, the observatory turned into an Eastern Bloc flying-saucer dome (the user's idea). With it every age-7 building the user wanted is done (Lumber Mill and Dock left as they are); none seen in game yet.
     - DONE 2026-10-29: Missile Silo 7-8 (`nukesilo_6.tga@7`/`@8`, `gemini/silo_age7-8_v5.jpg`, prompt from scratch after the user's own tries were too busy): a plain 1950s silo, open shaft and lid rail. The lid is the game's 3D addon (`nukesilo_addon.bh3`, node "door 01"), so the new shaft was moved onto the original's (centre about (115, 60) in the 262x155 sprite; scaled 0.85 around it so the left edge stays on the canvas). To check in game: the lid slides over the new shaft.
     - DONE 2026-10-29: Airbase 7-8 (`airbase_6.tga@7`/`@8`, `gemini/airbase_age7-8_v1.jpg`, first try): primitive 1950s communist airfield, layout kept (tower, arched hangar, runway slabs); scaled 0.88 around the tower base onto the original's. The band was picked by shape (long and thin, PCA eigenvalue ratio > 25): rust on the roof and orange crates have the same reds.
     - DONE 2026-10-29: one colour grade for the 13 communist age 7-8 sprites (`roman-buildings/tools/grade_sprite.py`: grey cast removed, chroma <= 16, luminance 105 / contrast 46, the Major City and University being the reference; the user: "propunerea ta este buna la toate"). Each Gemini picture came with its own cast (the cities brown, the church dark). University: foliage deepened afterwards (the user found it cartoony). Old versions in `romanian-buildings/work/before_grade/`. Market re-keyed with `make_sprite_white --tol 60` (white left round the edges and under the canopy).
     - DONE 2026-10-29: Fort and Tower 7-8 (`fort_6`, `tower_6`) graded to the same grey as the rest (option 3 of `work/fort_tower_options.png`, the user's choice; grade_sprite chroma 10); their fitted ground shadows kept. Old versions in `work/before_grade/`.
     - REDONE 2026-10-29: Small City 7 composed in code from separate Gemini pieces (`romanian-buildings/work/compose_village7.py`; `gemini/small-city_age7_piece_block1.jpg`, `gemini/small-city_age7_pieces_sheet.jpg`): an 8-storey block left, a 4-storey block back, a 3-storey block right, the Alimentara front-left, a transformer hut front; the yard empty. Each piece scaled once from the big picture (sharp), red roof edges thinned to 4 rows of player colour, then grade_sprite. Lessons: whole-estate pictures from Gemini came out hazy after a few edit rounds, with blocks hiding each other; generating pieces on one sheet ("each standing alone, none touching") and composing them works; no antennas (they come out as pixel lines). The long block on the sheet was bent, unused. Old version in `work/before_grade/village_6_age7_gemini_whole.tga`.
     - FINAL 2026-10-29: Small City 7 = `gemini/small-city_age7_v6.jpg` (the user: "ia asta si gata"). What finally worked: ONE image (the game's own village_6) and ONE short sentence in a new chat ("Reimagine this as a grim Romanian communist housing estate of grey prefab panel blocks, keeping the same layout, camera angle and style"), then one short removal edit (terrace, bushes, antennas). It has no red: the player colour is a thin line under each roof's front edges, found per roof in hand-set boxes (scratchpad roofband2.py idea). The code compositions from pieces were rejected by the user.
     - SEEN IN GAME 2026-10-29: the Major City of age 7 (`metro_6_age7`), the user: "orasul mare arata bine".
     - Player colour on the age-6 cities: a band of red glazed tiles under the eaves. A review page of every building Romania shows per age: `romanian-buildings/work/all_buildings.py` (same scale and ground point per row, footprint diamond).
     - PAUSED 2026-10-28 (the user: "nu lucram la age 8 momentan"): cities of age 8 (`village_6.tga@8`, `city_6.tga@8`). Until then age 8 shows the game's own city. Tried: the same blocks after 1989, renovated piecemeal (Claude's proposal); with the age-7 picture as "the same place", Gemini returned almost the same picture (a few air conditioners and dishes, `gemini/small-city_age8_v1_too-close-to-7.jpg`). If resumed, ask for the change as a big, concrete edit (two thirds of each facade in coloured insulation patches).
  4. Unique units (see `romania/README.md`).
     - TODO (the user, 2026-10-29, from a game screenshot of the Auto Plant): change the TR-77's icon (`romania/icons/tank.png`, cell 0,7,5 in `romania_units.ini`; the Gemini sheet `romania/gemini/sheet5_modern_armour.jpg`). When a unit gets a model of its own (R-2, IAR-80), its icon should be redone to match that model too: a render of our own model in the icon style.
     - Seen in game 2026-10-29: Romania plays (city Bucuresti, the Auto Plant builds the TR-77 at 75 metal / 65 oil, hits 195, attack 19, upgrading to the TR-85M1 Bizonul in the Information Age).
     - TODO (the user, 2026-10-29): the Palace of the Parliament (Bucharest) is to be a Romanian Wonder, not the Senate. Senate and University of Romania are urgent (the user); the Senate now shows the British Parliament with Big Ben (`senate_3n`, ages 4-8); proposed for it: the Palace of Culture in Iasi (neo-Gothic, central clock tower), awaiting the user's choice.
     - TODO (the user, 2026-10-29): Romania's Caravan trucks need their own look (now the European truck). Candidates: the TV-4 / TV-5 van (Uzina Tudor Vladimirescu, later Rocar, from 1958; TV-41 / TV-51 from 1964), "the first Romanian van", for a van; the SR-101 truck (Steagul Rosu, Brasov, 1954-1960, a licensed ZiS-150) for 1945-1960 if a lorry fits better. Same route as the R-2: Gemini picture, the user's Tripo model, our reduction onto the game truck's skeleton. DECIDED (the user, 2026-10-29): the ZiS-150, i.e. Romania's SR-101 (Steagul Rosu, Brasov, 1954-1960).

## Romans

- **Buildings now:** ours for much of ages 1-8, the rest Greco-Roman / Mediterranean.
- **Units:** European style. Their modern infantry and tanks are the German WWII models.
- **Tower 1-3 redone 2026-10-27** (`roman-buildings/gemini/tower_age1-3_v3.jpg`): the old one (`work/before_tower_v3/`) was too clean and brighter than the game (`warm.py` had brightened it; no more warm.py). Now a rough log frontier watchtower with an open lookout, no warm pass, scaled 0.85 to leave room above the roof for a waving flag in the player's colour: 12 frames (`tools/flag_frames.py`), rule `tower_0m.tga~12:1200:loop`. Gemini lessons: "primitive and battered" made a ruin (the user: "primitiv nu ruina"); a closed upper storey is wrong for a watchtower; a painted band on wattle looked odd, a red cloak over the parapet worked.
- **Done:** Barracks 1-3 and 4-5, Tower (all ages), Small City 1, 2 and 3, Large City 3, Small City 4-5, Large City 6-8, Market 1, 2 and 3, Fort 7-8 and the Data Center (`roman-buildings`).
- **To improve:**
  1. Test the depth meshes in game (see "Roman art" in `TODO.md`).
  2. Ages 1-3, in this order: houses (`house1-6_0m`), Temple, Library, Granary, Lumber Mill, Smelter, Stable, Siege Factory, Dock, Fort, University, Lookout.
  3. Small City 6-8, Large City 4-5, Major City, Fort 4 and 5-6.
  4. Snow versions for winter maps (`<name>_snow.tga`).
  5. Fighter: Bf 109 -> Macchi C.202 (new art). The user does not want a Roman Strategic Bomber (2026-10-25).

## Russians

- **Buildings now:** ours for the base pack in ages 1-5, plus some towers and forts. Generic modern in 6-8, except our Temple and Fort.
- **Units:** European style. Their attack helicopter is the Hind.
- **Done 2026-10-24/25** (`russian-buildings`):
  - Ages 1-3: Small City, Large City, Temple, Barracks, Market, Library, Fort, Tower.
  - Ages 4-5: Small City, Large City, Barracks, Market, Library.
  - Temple 4-8, Fort 4, Tower 4, Fort 7 and Fort 8.
- **To improve:**
  1. Test in game. Check that the cities' animated addons (`village_3n_addon`, `City_3N_ADDON`, `village_0ma_addon`, `city_0ma_addon`) do not float over the new art.
  2. Fort 5-6 (`fort_4`) is parked after several tries. The best one, a Petrine fort with a commandant's house (`russian-buildings/gemini/fort_age5-6.jpg`), has cannons far too small, and its spire shrinks the whole fort in the frame. No star shapes: the user dislikes them.
  3. If wanted: Granary, Lumber Mill, Smelter, Stable, Dock, houses.
  4. Aircraft: Fighter P-40 -> Yak, Strategic Bomber B-52 -> Tu-22M Backfire (mask changes only). New art: Mi-8 helicopter.
- **Gemini tips:**
  - A style image of a whole town makes Gemini paint the new building inside that town. Without a style image the building turns cartoony.
  - What worked: crop the good building out of a failed attempt, then ask in a new chat to isolate it.
- **Later:** balance, the Cossacks may be too strong.

## Spanish

- **Buildings now:** Greco-Roman (1-3), Mediterranean (4-5), Mediterranean modern for the cities (6-8), shared with the Greeks and Romans.
- **Units:** European style. Their modern infantry and tanks are the German WWII models.
- **Done:** Senate, ages 4-8 (`senate_3m`, the Mediterranean senate is the same from age 4 to 8), 2026-10-26 (`spanish-buildings`): the Alcázar of Segovia, the royal palace Philip II rebuilt with slate spires: the Juan II keep with corner turrets, a long wing with slate roofs and pencil spires, round towers with conical roofs, an arched gate. Scale 0.82, placed by the original's front corner (the mesh diamond of `senate_3m` does not match the original art). Player colour: a band along the low front wall and the cloth over the gate.
- **Gemini tips (senate):** El Escorial came out well but nearly square, as the real one is, while the senate footprint is 4x7; asking for a longer building, giving a grey volume mock-up next to the picture, or the mock-up alone did not help (it copied the square, then drew a broken complex). Pick a building that is long by nature for a long footprint.
- **Dropped (2026-10-26):** Temple 4-8 as Seville Cathedral with the Giralda. The first image had a fine Giralda but a Gothic cathedral full of pinnacles and flying buttresses (busy at game size); asking to simplify it gave a flat, shapeless box. The user gave it up. Lesson: Gothic cathedrals are too busy for a 4x4 sprite.
- **To improve:** they share their buildings with the Greeks (and the Romans until those are done) in every age. They need their own style per age, for example Moorish / Reconquista, Spanish Golden Age, then modern Spanish.
- **Later:** balance, the Tercios may be too weak.

## Turks

- **Buildings now:** Aztec (1-3), their own Ottoman style (4-5), Ottoman modern for the cities (6-8).
- **Units:** European style. Only the Janissaries and the Basilica guns are their own.
- **To improve:**
  1. Ages 1-3 base pack (now Aztec), for example Seljuk.
  2. Jet fighter: Tornado -> an F-16 in desert colours (a texture only).
