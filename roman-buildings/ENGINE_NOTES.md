# Engine notes for per-nation building art

## Depth (zbuffer) meshes

- Every `<BUILD>` in `building_graphics.xml` has a `zbuffer=".\art\<name>z.bh3"` mesh besides its sprite texture. For example, tower ages 7-8 = `tower_6.tga` + `tower_6z.bh3` (149 vertices, 115 triangles, a stand on legs).
- The sprite does not write depth. Depth comes only from that mesh. A sprite pixel outside the mesh's screen shape keeps the depth of whatever was behind it, so anything drawn later wins there: farm crops, water (blended), units.
- With replaced art (`nation_art`), every pixel of the new building outside the old building's mesh shows this. Seen in game, 2026-10-24:
  - the Roman tower 7-8 under crops and water, with the old lookout's legs visible as the only solid part;
  - the Roman Large City 6-8 under a farm behind it.
- The fix needs a depth mesh that fits the new sprite. It must be swapped per nation like the sprite, because a bigger mesh shared by all nations would hide units behind empty space around the original buildings.
- Done 2026-10-25 (untested in game): `nation_art.c` loads `<replacement>z.bh3` into an RData of its own and swaps its geometry packets in during `draw_build`; `tools/depth_mesh.py` makes the meshes.

- First game with the meshes (2026-10-26, Dutch, northern European Market 1-3): the log shows the mesh loaded (`market_0mz.bh3: load 0, mesh packets 000442 (original 000442)`), yet wheat from a farm behind the market still draws over the left part of its roof, in vertical strips. `depth_mesh.py`'s check picture shows that part covered by the new mesh. So either the swapped packets do not reach the depth pass, or the mesh's depth there is behind the crops. Open; needs a test in game, for example a mesh made deliberately too big, to see whether the swap changes anything at all.
- Cause found 2026-10-26 (same game, Small City 1-3: half the barn roof gone, wheat and the territory border showing through in vertical strips): **face winding**. The game culls back faces. Its own building meshes are stored with every triangle facing away from the camera in raw .bh3 coordinates (`village_0maz`, `market_0mz`, `temple_0mz`, `barracks_0mz`, `tower_6z`: 99-100 % of the area "away" against the view direction (-1, -1, 2)), because the load transform's scale (1, -1, 1) mirrors the mesh and flips the winding. `depth_mesh.py` wrote tops and -y walls facing the camera (culled in game) and -x walls the other way (kept): only the -x walls wrote depth, hence the strips. Fixed: all quads outward, then every triangle reversed; all meshes rebuilt (100 % "away", like the game's). The Field Fortification and Data Center Expansion meshes (`make_nest.py`) have both windings, so they were never affected. Also seen: the PreTransformBatchMethod reads slots 1, 6, 10 and 18 of the RData it is given, so the swap does reach the depth pass.

### Where the mesh lives (found 2026-10-25)

- `RData` (0xA4 bytes, allocated as 0xA8 with a refcount dword first; ctor 0x0091A340): `+4` RMap, `+8` `RPacket *[24]` indexed by the packet's `get_packet_type()`, `+0x68` parameters (`NamedArray<RParam>`, emptied at the end of `RData::load`), `+0x98` pointer to 0x30 bytes that `draw_build` fills each draw.
- Packet types: Vert3 1, Vert4 2, Vert3Scratch 3, Norm 4, NormScratch 5, Texcrd 6, TexcrdScratch 7, Diffuse 8, DiffuseScratch 9, Face 10, FaceScratch 11, VStream 12, IStream 13, Texture 14, Sprite 15, Hierarchy 16, Node 17, Transform 18, Animation 19, BoundingVolume 20, Event 21, AnimMesh 22, Sound 23 (`RData::add_packet` 0x009195D0 stores at `+8 + 4*type`).
- `init_build_data`: texture and zbuffer paths from the `<BUILD>` element, then `RData::set_param(key, RParam{ptr, type})` (0x0091A460, `ret 0xc`): zbuffer path (key 0x10CC, type 4 = String*), texture path (0x12BC4, type 4), an int `GraphicPieces+0xF54` (0x1040, type 1), a Transform (0x12BD8, type 3: `rotate_z(270)`, scale (1, -1, 1)), the sprite scales `GraphicPieces+0xF7C` (0x12BEC, type 3). Keys are Strings at `*(*(0x00C06378)+0x10) + key`. RMap: `GraphicPieces+0xA6C` with a zbuffer, `+0xA68` without. Then `RData::load` 0x0091A480 -> `RMap::load` -> each method's `load`.
- `PreTransformMethod::load` 0x0091DBE0: `BH3Method::load` reads the .bh3 into packets, then the vertices are baked with the Transform parameter and the pose of the Animation packet (slot 19, may be NULL).
- `draw_build` writes the building's place into the Transform packet (slot 18) and then executes the RData. `PreTransformMethod::execute` 0x0091D950 and `PreTransformBatchMethod::execute` 0x0091D340 read slots 1, 6, 10 and 18 at once (the batch copies the transformed vertices into its own buffers), so swapping slots for the duration of `draw_build` is safe.
- The batch holds at most 16383 vertices (three buffers); a mesh bigger than that would raise the game's error. The game's own building meshes have 100-300 vertices; the generated ones up to about 6700.

### Projection (fitted 2026-10-25)

- Mesh (bh3 units, z up) to sprite pixels, origin at `spritexoff`, `spriteyoff`: `sx = x0 + 0.70 (x - y)`, `sy = y0 - 0.31 (x + y) - 0.90 z`. Fitted on 11 buildings (towers, barracks, temples, markets, libraries, cities) against their own meshes. Note the minus before `C`: the side with larger x + y is the back. The earlier fit on towers alone (`entrenchment/ENGINE_NOTES.md`) could not see the sign, the towers being symmetric.
- The game's own meshes stay inside their sprites (1-9 % of the mesh outside) but cover only 50-95 % of them: low edges, trees and decorations are left out.
- A mesh outside the sprite hides crops, water and units behind empty space. `depth_mesh.py` therefore builds columns no higher than the sprite is solid above the cell's centre, back and side corners (0-1 % outside, 73-98 % covered). Rounding up instead covered 92-100 % but put 5-27 % of the mesh outside, a visible band around each building.
- The attribute names (`zbuffer`, `spritexoff`, ...) are not plain strings in the exe; they come from a string table.

## Size on the footprint (2026-10-26)

- `make_sprite.py` fits the new art to the original's height and lets the width use the whole canvas. Gemini's buildings are often bigger and more spread out than the game's, so the new sprite overflowed its footprint: seen in game on the northern European Small City (houses behind and past the city's diamond, empty ground on the right) and Library.
- `tools/footprint_check.py` draws the footprint diamond (the ground extent of the original zbuffer mesh) over the sprites and measures how far each sprite's lower outline reaches past the diamond's front edges and side corners, against the original. `--all` checks every sprite in `nation_art.ini`; `--shift` finds a move for art that sits off-centre.
- `tools/rescale_sprite.py` scales (and moves) a finished sprite around the diamond's centre, keeping hard alpha edges. Run it on the clean copy, then `write_like.py` and `depth_mesh.py` (no `weather.py` since 2026-10-28: the user does not want the wear filter any more).
- Applied 2026-10-26 to 19 sprites (scale 0.75-0.93; old versions in each mod's `work/before_rescale/`). The northern European Small City was also moved 40 px right and 12 px up. Still over: Roman Tower 4 (`tower_3m`, held at 0.75, 0.6 would make it too small) and Roman Barracks 4-5 (0.94, left as is).
- Art that sits too far back is not always fixed by a move down. The Mongol Small City (`village_0a`) fitted the diamond's width but its back yurts stood behind the back edge, seen in game 2026-10-26 (`footprint_check` overflow was negative: it only measures the front edges and side corners). The front yurt and the campfire already touched the bottom of the canvas, and the diamond's front corner (y 334) lies below the canvas (297 high), so a move down cut them off. Fix (`mongol-buildings/work/settle_village.py`): scale around the canvas bottom (0.9), so the back moves down more than the front, then move (12, 26); the campfire and stools, separate pieces at the bottom, are lifted out first and put back by hand. Look at the back edge in the footprint picture too, not only the overflow number.
- `--shift` can move art the wrong way. The Romanian Small City (`village_0ma`, 2026-10-27) took its proposal (-8, 18) and in game sat left of its ground, with empty ground on the right (the territory outline is centred on the `<BUILD>` spritexoff / spriteyoff, x 276, while the mesh diamond's centre is x 262). Fixed by matching the original's left and right extent instead: scale 0.9 around (262, 196), move (19, 6). Compare the new art's left and right edges and front objects with the original's, side by side.

## Colour

- Gemini renders the building lit by the magenta backdrop, so neutral stone, concrete and glass come out pinkish (blue and red above green).
- The game's art has warm neutrals: R-G about +5..+12 and B-G about -6..-17 on near-grey pixels.
- `tools/color_match.py` scales red and blue so the new sprite's near-grey pixels get the original's balance. `make_sprite.py` runs it on every output. All installed Roman sprites were corrected on 2026-10-24; the originals are kept in `backup_before_colormatch/`.
- `make_sprite.py` repaints pink edge pixels as sand. The lit folds of a red cloth are salmon (light red), so they are repainted too, and the cloth comes out as a red-and-sand lattice. Neither `_B` nor `team_bands.py` closes it. What worked for the Egyptian Senate (2026-10-25): take `_A`, draw a polygon around the cloth, make every bright building pixel inside it player colour, fill the holes, and take the grey from the red channel, which the repaint keeps (`egyptian-buildings/work/senate_team.py`).

## Light direction (measured 2026-10-27)

- The game's building art is lit from the upper right. On `fort_0m` the front-left wall averages about 59 in brightness and the front-right wall about 97, and the shadow (alpha 145) falls to the lower left of the building.
- Gemini prompts must name this light: right-facing walls lit, left-facing walls in shade. Without it, Gemini lights from the left or lights both faces evenly, and the building looks flat or pasted on.
- When Gemini casts a shadow on the magenta, it is the same magenta, only darker (min(R, B) well above G). `romanian-buildings/work/clean_background.py` flattens it, along with Gemini's black frame, before `make_sprite.py`, which would otherwise keep it as half-transparent building.

## Waving flags (2026-10-27)

- A building can get a waving flag in the player's colour without the game's addon system: nation_art.c already plays animated replacements (`<name>~<frames>:<ms>:loop = ...%d.tga`), so `tools/flag_frames.py` draws a pole and a flag (alpha 248, grey-shaded cloth) on every frame of a finished sprite. The flag must fit inside the original canvas (`write_like.py` keeps its size and origin): scale the building down first if its roof touches the top.
- nation_art.c loads the depth mesh next to frame 0 (`tower_0m_0z.bh3`) for every frame; `depth_mesh.py` now reads animated rules that way.
- Not yet seen in game (Roman Tower 1-3).

## BH3

Nested chunks of `[u32 size incl. header][u16 type][u16 n]`. Type 0 is the root, 1 a mesh, 2 vertices (u32 count, float4 each), 3 normals, 4 UVs, 5 triangles (u32 index count, u16 indices), 6 nodes.

Unit models are low-poly: the 802 BH3 files listed in `Data/unit_graphics.xml` have a median of 211 triangles (mean 237, 80% between 160 and 350). Infantry about 200 (legion 203, hoplite 209), cavalry about 300 (cataphract 304), general 331, bomber 403, sailing ships about 480, elephants with mahouts 630-670, the largest is the battleship at 713. Almost all are a single mesh.

Units are skinned rigidly to a shared skeleton (seen 2026-10-25 in `legion.bh3`):
- The type 6 node tree holds the bones (`Bip01 Pelvis`, `Bip01 Spine`, `Bip01 L Thigh`, ..., `Dummy..`, `ammo node 0`), each with a type 7 record that names a vertex index. The indices grow down the tree to the vertex count (172 for the legion), so each bone owns one run of vertices: one bone per vertex, vertices sorted by bone, no weights.
- `unit_graphics.xml` gives each unit a `skeleton=` (Biped 520, Knight 162, Machinegun 112, Siege 76, TrebHorse 54, ...) and names its animations by shared names (`<ANIM name="CHAR_WALK" file="Praetorian Walk"/>`, `.bha` files in `art\`). A new mesh on the Biped bones can reuse the game's existing animations.
- Unit textures are small: 64x64 (316 files), 128x128 (211), 256x256 (102). RGBA; the alpha seems to be the player colour mask (legion 4 % of pixels below 128, hoplite 17 %), not yet confirmed.
- Unit styles are separate from building styles: `<UNIT_CONTINENT>` in `tribes\<nation>.xml`. 0 European (`DEFAULT` art): Americans, British, Dutch, Egyptians, French, Germans, Greeks, Romans, Russians, Spanish, Turks. 1 Arab: Bantu, Nubians. 2 American (`AMERICAN`): Aztecs, Inca, Maya. 3 Asian: Chinese, Japanese, Koreans, Mongols. 4 Iroquois (`NA`): Iroquois, Lakota. 5 EIndian (`INDIA`): Indians, Persians. Generic units (Infantry, Musketeers, Pikemen, ...) have one model per style (`INFANTRY-ARAB-AGE0` = `inf_arab.bh3`).
- Unique vs generic art (2026-10-25, script in the session scratchpad, not kept): almost every unique unit has its own model. The ones that reuse the generic model: Romans, Spanish and Germans in the modern ages (`inf_german`, `panzer`, `hummel`, `flakgungerman`, `biplane_alb`); Egyptians' tanks, BMP and MiG; Bantu, Nubians, Chinese and Mongols' BMP / T-80; Americans' armored car and fighter; Korean citizens and scouts. Russians' T-80 has its own texture only.
- Aircraft have extra per-nation entries in `unitrules.xml`: the same `<NAME>`, `<GRAFT>` = that name, their own `<TRIBE_MASK>` and `<GRAPH>`. They sit beside the unit styles. Who gets what (2026-10-25, from the masks and renders with `roman-units/tools/bh3_render.py`):
  - Biplane: generic biplane; `ALBATROSS` for Germans, Spanish, Romans.
  - Fighter: `FIGHTER` = Spitfire (`ww2fighter`; Yak `fighter_yak` in the INDIA style, Mustang `fighter_na` in the NA style) for Persians, Dutch, Indians, Lakota, Iroquois, British, French, Greeks. `P40` for Mongols, Koreans, Chinese, Russians, Inca, Maya, Aztecs. `P40DES` (desert texture) for Turks, Egyptians, Nubians. `ME109` for Germans, Spanish, Romans. `FIGHTERZERO` Japanese. `MUSTANG` Americans.
  - Jet Fighter: `JETFIGHTER` = F-16 for the rest. `MIRAGE` French. `TORNADO` Germans, British, Romans. `TORNADODES` Turks, Nubians. `MIG` (MiG-29 look) Mongols, Chinese, Russians. `MIGDES` Egyptians.
  - Bomber: B-25 (`bomber.bh3`, `B-25.tga`) for all but the British (Lancaster); the AMERICAN style (Aztecs, Maya, Inca) gets the He 111 (`bomber_german`).
  - Strategic Bomber: B-52 for all; the ASIAN style (Chinese, Japanese, Koreans, Mongols) gets the Tu-22M Backfire.
  - Stealth Bomber B-2, Advanced Fighter (an F-35 / X-32 look) and Helicopter (`chopper.bh3`, a Bell 47) are the same for all. Attack Helicopter: Apache (desert texture `attackchopper_arab` in the ARAB style); `HIND` Russians.
- A reskin (new texture on the same mesh) needs no mesh work. A new model (for example from Tripo) needs: decimating to about 200-400 triangles, one bone per vertex, the vertices sorted by bone, the texture baked down to 64-256 px, and a BH3 writer (not written yet).

## Winter (snow) buildings

- `Data/overlayimages.xml` lists, for 208 building textures, a snow mask (`art\<name>_s.tga`) and a ready-made snowy texture (`art\snow\<name>_snow.tga`, `reprocess="0"`).
- A tileset with `snowbuildings="1"` (`Data/tilesets.xml`, the "Snowy" tileset) makes the game draw the `_snow` textures, so `nation_art` sees names like `city_6m_snow.tga`.
- `nation_art.c` now falls back to the rule for `<name>.tga` when there is no rule for `<name>_snow.tga`. A snowy Roman version only needs its own rule, for example `city_6m_snow.tga = nation_art\romans\city_6m_snow.tga`.
- Snowy versions can be made by asking Gemini to add snow to the Roman images (same camera, magenta background) and running them through `make_sprite.py`.

## Building styles shared between nations

- Each nation picks a style with `<BUILD_CONTINENT>` in `tribes\<nation>.xml`: 0 Med (Greeks, Romans, Spanish), 1 NEuro (British, Dutch, French, Germans, Russians), 2 Asian (Chinese, Japanese, Mongols), 3 Korean, 4 American (Aztecs, Inca, Maya), 5 Turk (Bantu, Egyptians, Nubians, Turks), 6 Iroquois (Iroquois, Lakota), 7 Colonial (Americans), 8 EIndian (Indians, Persians).
- `Data/building_graphics.xml` gives each building a texture per style and age (`<BUILD name="VILLAGE-NEUROPE-AGE3" texture=...>`; `DEFAULT` is the Med style). An entry holds until the next listed age: AGE0 = ages 1-3, AGE3 = age 4, AGE4 = age 5, AGE5 = age 6, AGE6 = age 7.
- Ages 1-3 have only four sets: Med (`_0m`, also used by NEuro and Colonial: 9 nations), desert (`_0s`, used by American, Turk and EIndian: 9 nations), Asian (`_0a`, also Korean: 4 nations) and Iroquois (`_0i`: 2 nations).
- Age 4 is where the nine styles differ (about 24 of 28 textures are the style's own).
- From age 6 on, 13 of 17 textures are the same for every style (only the cities, library and temple differ), and the age 5 and 7 entries are identical for all styles.

### Addons (animated parts drawn over a building)

- A `<BUILD>` can hold `<EXISTEVENT subject="<AMMO name>" anim="<ANIM name>">`: a small 3D model (`effects_graphics.xml` `<AMMO model= texture=>`) with a skeletal animation (`anim_graphics.xml` `.bha`), drawn on top of the sprite at the building's origin. The waving cloths of the Med Small City are `village_0ma_addon.bh3` (12 bones, texture `city_0ma_addon.tga`: cloth strips).
- The same texture can carry different addons per style. NEuro, Colonial and Iroquois Small and Large Cities of ages 1-3 (and the age 6+ Large City) use `FORGE_0M_ADDON`: a placeholder, one triangle 60 units under the ground and a root with two bones (z 63.5; y 49), nothing visible. Those styles show no cloths on `village_0ma` / `city_0ma`, only the `PARTICLEEVENT`s (chimney smoke at nodes 0 and 1, `CAMPFIRE_SMOLDER` at node 2 of the Small City).
- A nation's own art (nation_art.c) keeps the addon of its style's entry. To move or add an addon per nation, the EXISTEVENT model would have to be swapped like the depth mesh; not done.

## Farms (crops)

- A farm has no building sprite: `<BUILD name="FARM-DEFAULT-AGE0" texture="">`, so the `draw_build` hook never sees it.
- The ground is a terrain underlay (`wheatfarm` in `Data/tilesets.xml` and `Data/terrainunderlay.xml`, `farm_wheat.tga`, furrows). It is the same for every nation.
- The crop is made of stalks the engine generates. `<FARM texture=".\art\farm_wheat_1.tga" stalk_red/green/blue/alpha min_height="10" max_height="36">` in `Data/building_graphics.xml`, and each tileset's `<FARMTEX>` picks the texture (`farm_wheat_1`, `farm_jungle_wheat_1`, `farm_snowy_wheat_1`; 128x128, two stalk strips). `TileSet::get_wheat_texture` 0x00878190.
- `FarmsOut::draw` 0x008D75F0 (called from `Scene::render`) walks the visible farm render objects one by one, builds the stalk vertices (`SimpleArray<G>::append_with_offset`) and draws them. That loop is where a crop for each nation would go: give each farm the texture, or a column of a larger texture, for its owner's nation. Not yet worked out: where the texture is bound and where the stalk UVs are computed.
- Farm logic is in `Farms` (`grow` 0x008D91C0, `snip` 0x008D9240, `recalc_heights` 0x008D88A0) and is part of the simulation. A crop mod must change only `FarmsOut` (drawing), so multiplayer stays in sync.

## 3D render framing (2026-10-26)
`tools/trellis/render_iso.py` centred the model in 3D, then turned and tilted it. After the tilt the 3D centre is not the centre of the screen, so a tall model sat low and its front corner fell off the bottom of the picture (the Nubian pyramids lost the front of the right pyramid, which stayed in the sprite through make_sprite and the rescale). It now centres the projected model on screen and scales depth into the clip range, so no part is cut or clipped. Check every render for a flat cut along an edge of the picture before making a sprite.

## Overhangs and the depth mesh (2026-10-29)

- Romania's modern tower (`tower_6`) has a top wider than its shaft. The depth mesh's columns stand on ground points, so they covered only the shaft: in game the wheat field behind drew over the overhanging top ("ciuntit"; `work/depth_tower_6.png` showed it red). `depth_mesh.py` now adds, for the sprites in `CARDS`, upright quads facing the camera in the plane x + y = (the old mesh's middle depth) over every uncovered sprite pixel (`cards()`). Coverage went from about 90% to 98-99%. The name `tower_6` matches the Roman and other nations' modern towers too, so all of them got the cards.
- Lost again the same day: the 500-vertex cap (below) added the cards only when columns + cards fitted, and dropped them silently otherwise. One card per pixel run gives 300-500 vertices on the tower's ragged edges, so all three towers were rebuilt without cards. Seen in game: trees behind Romania's tower drew over the right side of its top and the end of the red band. Now the uncovered pixels are snapped to blocks (`blocks()`, 1-8 px) and every grid step is tried with every block size; the best cover under 500 vertices is kept. Roman, Dutch and Romanian towers: 52-124 card vertices, 362-469 in all, the overhangs covered. Check `work/depth_<name>.png` after every rebuild: red on the top means no depth there.

## A sprite bigger than the original texture (test, 2026-10-29)

- The plot paved in game is about 1.2x the zbuffer mesh's ground diamond (`footprint_check`), calibrated on the user's screenshots by template-matching the sprite and laying the scaled diamond over the plot border. For the cities and the modern fort that plot reaches past the right and bottom edges of the original texture (`city_6` 570x431: plot x -58..670, y to 477), and the new art already touches those edges, so it cannot be moved down or right inside the same canvas.
- Test: Romania's `fort_6.tga` was given a canvas 10 px wider and taller with the art moved (10, 10) (`romanian-buildings/work/fort_6_before_move.tga` is the old one). If the game draws a building sprite from its top-left corner at the BUILD entry's offsets with the texture's own size, the fort shows 10 px lower and further right; if it centres the texture, 5 px; if it stretches it to the original size, it looks squashed (then revert). To check in game.

## Air defence structures (2026-10-29)

- The Observation Post upgrades to Air Defense Gun (Industrial Age) -> Radar Air Defense (Modern) -> SAM Installation -> Missile Defense (`buildingrules.xml`, FROM Observation Post; help: "Upgrades an Observation Post to an Air Defense Gun, Air Defense or SAM Installation"). They have no sprite: their `<BUILD>` entries in `building_graphics.xml` have no texture, and they are drawn as 3D models from `unit_graphics.xml` (`airdefensegun.bh3`, `radardefensegun.bh3` scale 1.35, `sam.bh3`) on the sandbag ring `Airdef Base` (`airdefensegun_base.bh3`, `effects_graphics.xml`, an EXISTEVENT at node -1). So sprite tools (nation_art, the collages) never see them; `romanian-buildings/work/aa_sprite.py` renders one for the town collage. A nation-specific look would be a unit_graphics change, as for units.


## The plot is the tile square, not the zbuffer mesh (2026-10-29)

- A building's paved plot is its tile square (`mask="NxN ..."` in `building_graphics.xml`, the same N as `X_SIZE` in `buildingrules.xml`) centred on its position; the sprite's origin (`spritexoff` / `spriteyoff`) is that position. About 58 world units a tile (the Small City, 7x7, measured on a game screenshot).
- The zbuffer mesh only covers where the art stands, and need not be centred: the age 5-8 Lumber Mill's `sawmill_6z.bh3` lies x 10..209, y -126..116, about 110 units (two tiles) off the centre, while most meshes (village_6, granary_6, market_6, smelter_6) are centred within ~15 units. So `footprint_check.diamond()` (mesh-based) is the plot only for centred meshes. The town collage (`romanian-buildings/work/age_town.py`) drew the Lumber Mill's plot two tiles off until it was changed to the tile square.
- The mask is `AxB`: B tiles along world x, A along world y (the Senate, `4x7 extra space`, is long in x like its mesh; the Airbase is `5x9 solid`). TILE = 58 world units checked by laying the tile rectangle on two game screenshots (Small City 7x7, `5.png`; Fort 5x5, `18.png`): it falls on the painted plot border in both. `halfoffset` (a half-tile shift for some sizes?) not yet looked at.

## The depth meshes' vertex budget (2026-10-29)

- Crash in a Romanian age-7 game: breakpoint (0x80000003) at 0x0091A213 in `RMethod::method_error`, called by `PreTransformBatchMethod::execute` 0x0091D340 when none of its 3 batch slots has room (count + mesh vertices < 0x3FFF each): the "too many verts for Pre-Transform Batch Transform" error, the same as the R-2's (romania/ENGINE_NOTES.md). The zbuffer meshes go through that batch (`PreTransformMethod::load`), and 77 of our 98 depth meshes had 600-6349 vertices (the game's own: at most 1096, median 129), so a screen of our buildings filled it.
- `depth_mesh.py` now caps every mesh at 500 vertices (coarser grids, steps up to 40 units); all 98 rebuilt and installed, largest 494. Keep new meshes (and unit models) near the game's sizes.
