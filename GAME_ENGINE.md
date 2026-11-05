# How Rise of Nations works: the whole engine in one place

This page brings together everything the RON Revived work has found about *Rise of Nations: Extended Edition* (Steam). It says how each part of the game works and where the full details are. The per-mod `ENGINE_NOTES.md` files keep every address and test story; this page is the map to them.

- **Exe:** `riseofnations.exe`, Steam EE, timestamp **6674863F**, base 0x400000, 32-bit, not large-address-aware (characteristics 0x102), so the process has 2 GB of address space; install.ps1 sets the flag (0x122, logged in RON_Revived_laa.txt), giving 4 GB on 64-bit Windows.
- **Symbols:** `sbl/rise_z.map` (names for every address); `sbl/rise.pdb` also ships. `d3dgl.dll` (timestamp 667485A4) has `sbl/d3dgl.map`.
- **Addresses:** all are virtual addresses in the exe as the map gives them (`0x00xxxxxx`). The DLL adds the ASLR slide at run time.
- Source keys used below: **CS** = [camera-smooth/ENGINE_NOTES.md](camera-smooth/ENGINE_NOTES.md), **DC** = [data-center/ENGINE_NOTES.md](data-center/ENGINE_NOTES.md), **EN** = [entrenchment/ENGINE_NOTES.md](entrenchment/ENGINE_NOTES.md), **PD** = [paradrop/ENGINE_NOTES.md](paradrop/ENGINE_NOTES.md), **RB** = [roman-buildings/ENGINE_NOTES.md](roman-buildings/ENGINE_NOTES.md), **RO** = [romania/ENGINE_NOTES.md](romania/ENGINE_NOTES.md), **NM** = [nation-music/ENGINE_NOTES.md](nation-music/ENGINE_NOTES.md), **SV** = [survival/ENGINE_NOTES.md](survival/ENGINE_NOTES.md), **UI** = [ui-theme/ENGINE_NOTES.md](ui-theme/ENGINE_NOTES.md), **CB** = [tools/COMBAT_NOTES.md](tools/COMBAT_NOTES.md). "CS § Water" means the section "Water" in CS.

**Rule:** a new finding goes into the right mod's ENGINE_NOTES.md first. If it changes the picture here, update this page too.

---

## 1. Units of measure and encoded values

| What | Value | Source |
|---|---|---|
| Simulation rate | **15 frames a second**; rules.xml times are in fifteenths of a second; the game's own "minute" is 900 frames | CS § Nuclear weapons |
| Game time | `[game+0x550]`, frames, raised only in `Game::do_frame` (0x005924BF) | CS § Unit facing |
| Draw interpolation | `game+0x854` (float) = how far the drawn picture is between the last two frames (`TurnControl::get_delta_percent` 0x00957950) | CS § Unit facing |
| Script time | `[game+0x560]`: script `time()` = it / 60 in minutes | SV § Compiling |
| Frame-rate clock | `game+0x840 / +0x844` count 66 ms frames; not game time | CS § Loading screen |
| World distance | **192 world units = 1 tile** (Coord) | PD § Airdrop |
| Building art units | about 58 bh3 units per tile on the footprint | RB § The plot is the tile square |
| Angles | 32-bit binary angles (2^32 = 360 degrees) | CS § Unit facing |
| Damage | whole hit points plus sixteenths (`+0x3B` carries the fraction) | CS § Damage like attrition |
| Costs in XML | buildingrules COST and craftrules COST2 are in tens (Tower 80, Fort 400); SUPPORTVALUE is raw | CS § Range circles; DC § Expansions |
| Colour | `Color` is 12 bytes: dword 0xAABBGGRR, then 555 and 565 words. Statics: GREY 0x00C8D218, RED 0x00C8D248, BLACK 0x00C8D260, WHITE 0x00C8D26C (range 0x00C8D1D0 GREEN .. 0x00C8D278 YELLOW) | CS § Veteran ranks |

**Encoded values (anti-cheat XOR).** Many numbers are stored XORed with a key:
- object position `+0x10 / +0x14` (x, y; z at +0xC): `^ 0x63637`;
- treasury `[[leader+0x6EB8] + 4r] ^ 0x8221`;
- resource cap `+0x30+4r`: `(cap << 4) ^ 0x1281`;
- gather rate `+0x64+4r ^ 0x872` (rates are x16);
- shown "+rate" `+0x94+4r ^ 0x90236`;
- commerce-cap flag `+0x4C+4r ^ 0x8932`;
- the player's age `+0xDC ^ 0x62766`; age-like values `+0xE8 / +0xF0 ^ 0x63187`.

(DC § Players; DC § More findings; CS § Resource panel; RO § Unit names and icons)

Resource index everywhere: **0 food, 1 timber, 2 wealth, 3 knowledge, 4 metal, 5 oil**. Resources are themselves types 0..5, so `type_avail(r, 1)` asks whether a resource is in play.

---

## 2. The big picture

```
riseofnations.exe ──imports──> winmm.dll  (our proxy: 193 forwards + all hooks; CS README)
   │                            └─ reads ron_camera.ini, logs ron_camera.log, serves mod_override\ files
   ├─ CrossplayProxy.dll -> PartyWin.dll, PlayFabMultiplayerWin.dll (loaded at start)
   ├─ steam_api -> steamclient.dll, gameoverlayrenderer.dll
   ├─ d3dgl.dll   (the D3D11 renderer; shaders compiled at run time from shaders\*.hlsl)
   └─ xasl.dll    (sound: XAudio2 2.9, reads WAV / MS-ADPCM)

Data\*.xml (rules, units, buildings, crafts, graphics, sounds, interface, help, strings)
art\*.tga / *.bh3 / *.bha     sounds\*.wav     scenario\Scripts\*.bhs     tribes\*.xml
```

One simulation frame (`Game::do_frame`), in order: `Leaders::strategy_all` (AI planning) → `GameDaemon::process_all` 0x00732700 → `Armies::process_all` → **`Objects::process_all`** 0x0065DCE0 (units move, air physics) → **`Objects::inc_time`** (blasts, ammo flights) → `GraphicEvents::process` …; `Leaders::process_all` also runs every frame. (PD § Airdrop; DC § More findings; CS § Blasts)

Drawing (`Game::loop_render` → `Scene::render`) runs separately and blends between the last two simulation frames with `game+0x854`.

**The golden rule of multiplayer:** every machine runs the same simulation from the same commands. Anything that changes simulation state must be (a) the same on every machine, fed only by game state and the frame clock, never by the local camera, local player, or wall-clock time, or (b) sent as a command. Drawing-only changes are always safe. See § 6.

---

## 3. Files and data

### 3.1 How mods reach the game: mod_override

- The mods change **no game file**. Each data mod has a `mod_override/` folder holding the game's XML and art. The winmm.dll file hooks (`override.c`, CreateFileW and friends) serve those files instead of the originals.
- The package lays the folders over each other in build order: data-center, paradrop, entrenchment, general-heal, nuke-fx, ui-theme, romania, translations, nation-music. **The last writer of a shared file wins.** (README § Building)
- Folder listings go through `_wfindfirst64i32` → `FindFirstFileExW` (called at 0x00A2CE83, 0x00A2E147). `override.c` redirects exact-file searches; wildcard searches are not merged. A model that existed only in mod_override was invisible to the exact search before this fix and gave "NO DEFAULT UNIT GPIECE - VERY BAD" (0x0090C609). (PD § BH3 and BHA)
- `override_redirect` can also serve files the DLL writes at run time, e.g. fog ramps in `%TEMP%\RON Revived\` (CS § Fog of war) and replacement shaders (CS § Picture look).
- **Languages:** `XML::init` 0x00A279E0 adds the language suffix to every XML name (".7" German, "" English, ".10" Spanish, ".12" French, ".16" Italian) and uses that copy if it exists. An English-only override is therefore ignored in German, and every mod text vanished. `translations/` builds a `.N` copy of every file the mods change. (CS § Languages)

### 3.2 The type index space (one global numbering)

The exe **refuses data files whose type counts differ** from what it was built with. You cannot add new types; you reuse existing slots. (DC § Types)

| Range | What | Notes |
|---|---|---|
| 0..5 | resources | |
| 0x32..0x19D | units (364) | unit entry i of unitrules.xml = 0x32 + i. Citizen 0x32, Generals 0x36-0x39, Spy 0x3A, Supply Wagons 0x3F-0x41, Scout 0x45, Commando 0x49, Cruise Missile 0x13A, Nuclear Missile 0x13B, ICBM 0x13C, Senator 0x161, Blucher 0x175 / Kutosov 0x176 (campaign heroes), SAS 0x183 (our Interceptor) |
| 0x19E..0x21E | buildings (129, buildingrules.xml order) | Missile Silo 0x208, wonders 0x20E..0x21E, El Dorado 0x1C0, Washington Monument 0x1C1 (our Data Center), Lenin Statue 0x1C2, Courtyard Fountain 0x207, Missile Shield 0x2B5 (inferred) |
| 0x220..0x274 | technologies | Modern Age 0x225, Art of War 0x23C |
| 0x275..0x2AB | crafts / spells (craftrules.xml order) | Bribe 0x275, Pilfer 0x276, Counterintelligence 0x277, Rally 0x279 (unfinished, our General's Heal), Decoys 0x27A, Ambush 0x27B, Forced March 0x27C, Double Agent 0x27F, Paradrop 0x284; casters of craft 0x293 may build like Citizens |

Free slots to reuse: campaign-only units (indices 308-350, `WHERE None`, all-zero TRIBE_MASK); the disabled scenario buildings above; buildings 37-104 are scenario-editor Houses / Apartments / Offices (`PREQ0 disable`, in no city). Every graphics family is named by some unit's GRAPH, so a new look must take over a campaign unit's family. (DC § Types, § Expansions; PD § Airdrop)

Type data: `TypeData *` per type at `[[0x00C0A274] + 4*type]` (RO § Unit names); the unit-type table is also reached through `*0x00C0AB10` (CS § Hover highlight). The BuildType of a type is `[[0x00C061F4]+0x10][type]` (DC § The technician). `ObjectTypeData::is(line)` 0x0065F7D0 tests whether a type belongs to a unit line (CB).

Key **TypeData fields**: +0x10 tribe mask, +0x18 six base costs (`Type::load_cost` 0x00663BA0), +0x60 internal name, +0x74 shown name, +0x88 graphics family, +0x1E4 OBJ_MASK bits (0x8000000 = nuke), +0x1E8 ATTACK x10, +0x1F4 recharge, +0x1FC max range, +0x200 min range / SPLASH, +0x210 HITS per soldier, +0x218 domain (0 land, 2 Air), +0x234/+0x238 size, +0x2B4 FLAGS bits (0x20 'f', 0x20000 'r' siege, 0x1000000 'y' unique), +0x2CC ammo piece, +0x2EC mana, +0x2F0 POP, +0x308 UBER_SIZE (soldiers per squad). (CS § Veteran ranks, § Nuclear weapons; RO § Loading screen; CB)

### 3.3 The XML files

| File | Holds | Key facts |
|---|---|---|
| `rules.xml` | global constants, TRIBES, FORMATIONS, gameinfo_flags | Read into `Constants` **in file order, one dword per value** (DC § Which city). ATTRITION 48 (`+0xD3C`), ARMAGEDDON 4 / per nation 1 / per team 2 (`+0xD14/+0xD18/+0xD1C`), DECOY_TIME 2500, respond ranges (UNIT_RESPOND 12 tiles, DEFENSIVE 4, GUARD 8, BUILD 12, GATHER 32, AIRCRAFT 10, BOMBER 12), CITY_HEAL_RATE, ENTRENCHMENT_MODIFIER 2/3. `<TRIBES>` lists exactly 24. |
| `unitrules.xml` | units | WHERE = the building that trains it; **POP must never be 0** (AI divides by it); SUPPORT is added to the price; MANA = craft points; GRAFT = the generic a unique unit replaces; FLAGS `x` = a tank wreck that blocks; TRIBE_MASK |
| `buildingrules.xml` | buildings | RANGE `min-max`; COST in tens; SUPPORT0/1 + SUPPORTVALUE0/1 = a ramp cost per building already owned; BUILD_FLAGS `j` = one per city (0x200); `PREQ0 disable` (-2) = not in the build menu; GARRISON_MAX / SQUADS; the file's header comment explains every flag letter |
| `craftrules.xml` | crafts | **known to the code only by position** (0x275 + index): never reorder. FROM / FROM2 name a unit line by its first unit; building buttons are hard-coded (`FROM Airbase` shows nothing). FLAGS bits from 'a' at `+0x1C8`: b units, c buildings, d ground. COST2 = the tooltip price |
| `rules.xml` mapsizes | the 7 map sizes, `<DATA>` = side in WCoord blocks (4 tiles each): Tiny 40 ... Huge 90, Big Huge 100 | the LAST entry sizes the engine's coordinate tables, so it is the maximum; raising it raises the limit (CS § Map size) |
| `techrules.xml` | technologies | TRIBE_MASK strings too |
| `balance.xml` | fine-tuning on top of the hard-coded counter table | CB |
| `building_graphics.xml` | one `<BUILD name="X-STYLE-AGEn" texture zbuffer spritexoff spriteyoff mask halfoffset>` per style and age | an entry holds until the next listed age (AGE0 = ages 1-3, AGE3 = 4, AGE4 = 5, AGE5 = 6, AGE6 = 7; no AGE7 exists); DEFAULT = the Med style; EXISTEVENT / PARTICLEEVENT add animated parts and smoke (RB) |
| `unit_graphics.xml` | per-unit skeleton, `<ANIM name="CHAR_WALK" file=...>`, icon `tex_id/tex_col/tex_row` (TEX_UNITS_0/1/2 = `art/iface_units_01..03.tga`), icon tiers -AGE0/-AGE3/-AGE5 | |
| `effects_graphics.xml` | AMMO models, particle systems, `<UNDERLAYS>` (the code uses the first 20 by index: new ones go at the end) | CS § Underlays |
| `anim_graphics.xml` | .bha animations | |
| `soundtypes.xml`, `soundfiles.xml`, `soundglobals.xml`, `sound.xml` | sounds per graphics FAMILY; the wav list; global sounds (**XML order ≠ enum order**); music tracks per tribe / age / mood | § 12 |
| `interface.xml`, `topmenu.xml`, `setupwin.xml`, `options.xml` | windows, rects, IFACETEX pieces, fonts; the n-th `<OPTION>` = option code n | XML gives sizes; positions are often decided in code (UI) |
| `help.xml` | tooltips by key (`SETUPCHECK<id>`, `TRIBE<n>`, ...) | a missing key shows nothing; `set_help` takes a key, not text |
| `typenames.xml` | shown names, keyed by a hash of the NAME (`String::generate_hash` 0x00A1B6B0, formula in DC § Expansions) | |
| `internal_strings.xml`, `translated_strings.xml` | string tables (`int_str_array` `*0x00C06378`, `loc_str_array` `*0x00C8CD00`, 0x14 bytes per entry); markup `{` `}` title, `{*1` `{*2` colour | CS § Loading screen, § Veteran ranks |
| `tilesets.xml` | `snowbuildings="1"`, `<FARMTEX>`, `<FOGTEX>` fog ramps | |
| `overlayimages.xml`, `terrainunderlay.xml` | snow masks for 208 textures; the farm underlay | RB |
| `tribes\<nation>.xml` | BUILD_CONTINENT, UNIT_CONTINENT, BACKUP_BUILD_CONTINENT, `<LEADER>` names | RO |
| `masks.txt` (game root) | footprint masks | DC § Expansions |
| `taunt_categories.txt`, `taunts.txt` | taunt numbers → AI meanings; 100 lines, 76-89 ally replies | CS § Talking to computer allies |

**Bit-letter fields:** OBJ_MASK letters are `1 << (letter - 'A')` at `+0x1E4` (F = foot soldiers), with `1`-`6` for bits 26-31. TRIBE_MASK is a 0/1 string read high bit first (`String::binary` 0x00A1B080). (EN § Who may garrison; RO § Nation masks)

### 3.4 Game scripts (.bhs)

- Every folder under `scenario\Scripts\` holding a `.bhs` becomes a **Script** choice in custom game setup.
- The language is C-like: `scenario{}`, `run_once{}`, `trigger name(cond){}`, static variables, arrays, and `include` from `scenario\scriptlibrary\`. The compiler knows all 879 `ScenarioFuncSet` functions; `scriptfunctions.xml` lists only the editor's.
- Compile errors go only to `OutputDebugStringW` (`Compiler::comp_error` 0x009BED80): use a DBWIN listener. Files must be CRLF, or every error says "Line 1". Typing is strict (a bool does not go into an int). `rand_int(lo, hi)` is inclusive. At most 100 timers (`ScriptTimers::add_timer` 0x00A049E0). `is_on_screen` uses the local camera, so it differs between machines; `add_flag` does nothing in multiplayer.
- Scripts can switch AI parts off (`disable_production_ai`, `city_ai`, `combat_ai`, `disable_take_attrition`). `ScenarioFuncSet::disable_type` 0x009EA130 disables a type for everyone (type `+0x30` = -2).

(SV; CS § No Nukes check box)

---

## 4. Art and sound formats

### 4.1 Building sprites (TGA)
- Uncompressed 32-bit. **Alpha is a category, not transparency:** 0 empty, 255 building, **248 player colour** (grey, tinted in game), **145 shadow** (drawn black at one darkness). Edges cannot be anti-aliased. (RB § Colour)
- The game's art has warm neutrals (on near-grey pixels R-G about +5..+12, B-G about -6..-17). Light comes from the **upper right**: right walls lit, shadows fall lower left (a ray moves sx -0.18h, sy +0.08h per unit of height). (RB § Light direction)
- `Sprite::init_32` 0x0054C1D0 trims transparent rows / columns (`+0x48`, `+0x4C`). `Sprite::draw` 0x0053E260 adds the trim with a truncating `idiv` and draws at whole pixels, so trimmed sprites step a pixel while zooming: subpixel.c fixes it. Art bigger than the original is moved by lowering the trims (`nation_art.ini <file>, dx, dy`). (CS § Sprite trim)
- Winter: a tileset with `snowbuildings="1"` draws `art\snow\<name>_snow.tga`. (RB § Winter)

### 4.2 Models (BH3) and animations (BHA)
- Nested chunks `[u32 size incl. header][u16 type][u16 children]`. Types: 0 root, 1 mesh, 2 positions (float4), 3 normals, 4 UVs (V from the top), 5 u16 indices, 6 node, 7 per-node record (first vertex, count, name, quaternion, position), 8 animation track (BHA: one per node, 9 floats per key). (PD § BH3 and BHA; RB § BH3)
- Units are low-poly (median 211 triangles over 802 files), **rigidly skinned**: one bone per vertex, vertices sorted by bone, no weights. A new mesh on the Biped bones reuses the existing animations. Each node's vertices are in that node's own frame. Tank `.bha` tracks are offsets from rest. (RB § BH3; RO § Tank models)
- Unit textures 64/128/256 px RGBA; the alpha marks player colour (about 57-66 on the aircraft looked at). (PD; RO § Aircraft models)
- Helicopter rotors are not in the mesh: they are `EXISTEVENT type="CopterBlade" node="2"` effects spun by `helicopter_propspin.bha`. (PD § Helicopter rotors)
- Air-defence buildings have no sprite: they are unit-graphics 3D models on an EXISTEVENT base. (RB § Air defence)
- **Crashes:** crash.c's unhandled exception filter writes `crash\crash_*.txt` + `.dmp` and starts the Discord crash panel (`RON_Revived_crash.ps1`). Without it, a crash shows only in Event Viewer (Application, id 1000: module and fault offset). (CS § Crash reports)
- **Open file limit:** the exe, d3dgl and xasl.dll share ucrtbase.dll, 512 open FILE streams by default. Every `<SOUND>` of every player's tribe in sound.xml is opened at game start; past 512 the next model open fails as "X.bh3 does not exist for Transform Batch Transform". override.c `stdio_raise()` sets `_setmaxstdio(8192)`. (NM § Crash at game start)
- **Vertex budget:** the Pre-Transform batch has 3 slots of under 0x3FFF (16383) vertices. With no room the mesh is silently not drawn ("too many verts for Pre-Transform Batch", bp at 0x0091A213). Game building meshes are at most 1096 vertices (median 129); keep depth meshes ≤ 500 and tanks near 400. (RB § Vertex budget)

### 4.3 Depth (zbuffer) meshes
- Buildings are 2D sprites that **write no depth**. Depth comes only from the `zbuffer` BH3 mesh. Any sprite pixel outside the mesh's screen shape gets painted over by crops, water, units and trees drawn later. So every swapped sprite needs its own depth mesh (`roman-buildings/tools/depth_mesh.py`). (RB § Depth meshes)
- Loading: `RData::set_param` 0x0091A460 (Transform `rotate_z(270)`, scale (1,-1,1)), `RData::load` 0x0091A480, `PreTransformMethod::load` 0x0091DBE0 bakes the vertices. `draw_build` writes the position into packet slot 18; swapping packets during `draw_build` is safe. (RB § Where the mesh lives)
- The game culls back faces and the (1,-1,1) scale mirrors the mesh. So triangles must be stored **facing away** in raw .bh3 coordinates (view direction (-1,-1,2)). Wrong winding gives vertical strips where only some walls write depth.
- **Projection** (bh3 units, z up, to sprite pixels from spritexoff/spriteyoff): `sx = x0 + 0.70 (x - y)`, `sy = y0 - 0.31 (x + y) - 0.90 z`. Larger x + y is the back. Overhangs need camera-facing "cards". (RB § Projection, § Overhangs)
- The **plot** is the tile square `mask="AxB"`, centred on the sprite origin, not the mesh. `halfoffset` x 96 world units shifts the sprite half a tile on both axes. (RB § The plot; DC § Expansions)

### 4.4 Icons and interface atlases
- Icon pages: 61x39 cells on a 62x40 step (TEX_UNITS 512x512; TEX_BUILDINGS_3 = `iface_buildings_wonders.tga` 256x256, 4x6). In TEX_ORDERS_0 (`iface_unit_orders.tga`) a cell no craft names may still be drawn from code: (7,4) is the cities' rally-flag button. (DC § Expansions; EN § Untargeted crafts)
- `GraphicPieces::update_all_icons(player, tribe)` 0x0090D3F0 fills 3 icon slots per type per player, from the player's unit style, only for tribes below 24. (RO § Unit names and icons)
- Interface pieces are named regions of shared atlases (`wrapper_pieces_01/02.tga`, `iface_diplomacy_01.tga`, `iface_unit_orders.tga`, `iface_AS0_assets.tga`). Replacing an atlas reskins every window that uses it. Textures used by in-game popups must be in the `IFACERENDERMANAGER` list, or they draw as white quads. (UI § Shared atlases, § Dialogs)
- Interface numbers are per-character glyph quads from a Text file ("interface") via `IFaceRenderManager::setup_text` 0x0051DD50. A non-integer scale makes holes; hud.c draws its own digits at whole-pixel scale. (CS § Resource panel and counters)

### 4.5 Sound
- Everything shipped is WAV PCM 16-bit at about 22050 Hz; music is stereo, nine tracks are 44.1 kHz. `xasl.dll` also decodes **MS-ADPCM** (tag 2) with 32..512 samples per block; ffmpeg cannot make those block sizes, hence `nation-music/tools/adpcm.py`. ADPCM is decompressed whole in memory (a 6 MB track takes about 25 MB). (NM § Formats, § Crash at game start)

---

## 5. The process, startup and how we hook it

### 5.1 The winmm.dll proxy
- The exe imports winmm (for `timeGetTime` and timers only). Our `winmm.dll` forwards all 193 exports to the system one. On load it checks the **exe timestamp and the bytes at every hooked address**, and installs nothing if anything differs, so a game update disables the mod instead of crashing it. Hooks are in-memory; the exe on disk is never touched. (CS README)
- Patches that must land before static constructors (`Tribes::Tribes`) run from `DllMain`. (RO § 24 nations)
- **Startup hang fix:** since about 2026-10-21 (Windows bcrypt 10.0.26100.9711 plus the new Steam client) the plain game could deadlock at start. The main thread's SteamAPI_Init loads steamclient.dll under the loader lock and waits on bcrypt's provider lock, while a PartyWin.dll thread holds that lock and waits for the loader. One `BCryptGenRandom(NULL, .., SYSTEM_PREFERRED_RNG)` in our DllMain warms bcrypt before any thread can race. (CS § Startup hang)
- Build: `camera-smooth/build.ps1` (zig from `pip install ziglang`, links bcrypt); `tools/gen_winmm_proxy.py` regenerates the export list; then each mod's `build_data.py`; then `package/build_package.py`. (README)

### 5.2 Launcher (patriots.exe)
- Its own process (timestamp 597669ED). It does not import winmm; it gets our DLL only when started through Steam, because the overlay loads winmm from the game folder. Play starts riseofnations.exe and the launcher exits. (UI § Launcher; CS § Updates)
- MFC, two windows: a layered PNG background (752x732) and controls dialog 102, colour-keyed and owned by it. Captions come only from `launcher_res\locale.ini`. The game's settings (language, window size) are in `%APPDATA%\Microsoft Games\Rise of Nations\rise2.ini`. (UI § Launcher)
- The Update plate fetches `releases/latest/download/RON_Revived_update.txt` (a plain redirect, no API rate limit). It must wait for patriots.exe to exit before replacing winmm.dll, since the launcher has it loaded. Part hashes are over file paths + SHA-256, so an unchanged part is not downloaded again. (CS § Updates)

### 5.3 Startup sequence
`Game::run` 0x00584590: `LoadingWin::exec` → `Game::init` (0 = success; calls `init_rules_and_teams`, `Leaders::init` 0x006ED850) → `clock_init` → `solo_init` → tips window → `CommandManager::start` 0x00942E10 (in multiplayer: SyncPoint, TimeSync, then closes the loading window, `game+0x821 |= 1`) → `game+0x558 = 1`, and the game loop runs. Before that flag, `Window::modal_idle_func` (`System::modal_idle` 0x005994A0) draws every window and presents, so a `modal_idle(); Sleep()` loop keeps a screen alive. `Window::set_modal` 0x00A55460 is the modal loop. (CS § Loading screen)

For tests: `autostart.ini [CONTROL] USEAUTOSTART=1` (with `[qol] play_button=0`), `rise2.ini StartReveal=1`, `SkipIntroMovies=1`. The game draws nothing while its window is not in front. (CS § Water)

### 5.4 Hooking techniques that worked
- **Re-target one call site** (`E8 rel32`): the most common and safest (bribe.c, follow.c, nukefx's add_nuke at 0x00678516, ...).
- **Entry hooks** on known prologues (e.g. `Object::fire_ammo` `55 8B EC 83 EC 54`).
- **Mid-function detours** of a fixed byte count, with a stub that redoes the overwritten instructions.
- **vtable slots**: Console `on_button_clicked` (+0x2D8 in vtable 0x00B25354); a **private vtable copy** for one object (nonukes.c copies the CheckBox vtable 0x00B22A90, 0x600 bytes, from the RTTI slot at -4).
- **Patch a lone empty function**: `Console::on_redraw` 0x007D0250 is a bare `ret` referenced only from vtable slot 0x00B2540C, so a `jmp` there gives a per-frame draw over the map.
- **Byte patches**: je→jmp, dropped jne, immediates (e.g. four 999s → 0x7FFFFFF0, zoom floor at 0x00844001).
- **Wrap and fix the result** (pop cap, resource caps), **return early** (`Build::finished` returning 1), **gate on the caller's return address** (`type_avail` from `Leader::create_buildings`).
- **Shared hooks**: one hook (e.g. `Objects::process_all`, `BuildOut::make_options`, `Options::add`, `type_avail`) serves several features; it is installed when any of them is on. The `Objects::process_all` post-hook in airdrop.c drives nukefx, nonukes and cityrepair each frame.

Calling conventions to watch: most methods are thiscall (ecx = this) with callee-pop `ret N`. Some take floats in xmm registers (`add_smooth` 0x0051B700 x/y in xmm2/xmm3; `draw_char_earth` x/y/t in xmm1..3; `add_explosion` in xmm1..3). `world_to_console` 0x00583840 takes out-pointers in ecx/edx and is caller-pop. `draw_circle` 0x008C6040 takes radius in ecx and type in edx. (CS various; PD)

### 5.5 Diagnosing crashes
The Windows Application event log gives the fault offset; **offset + 0x400000 = map address**. Dumps go to `%LOCALAPPDATA%\CrashDumps`. The mod logs to `ron_camera.log` (the line `tribe N age 1 draws sprite` tells which nation was played). Tools: `tools/rdis.py` (find, disassemble, xref with map names), `tools/istr.py` (internal string offsets), `tools/type_fields.py` (which XML tag fills which TypeData field, in `UnitType::init` 0x0061AB50 order), `tools/emu_damage.py` (runs `Balance::type_damage` under unicorn). (DC § Crash found; CB)

---

## 6. Simulation, sync, saves and replays

### 6.1 What is simulation state (must match on every machine)
- Everything in objects, leaders, the treasury, ammo, the nuke tables (`Nuke::add_nuke` runs inside `Objects::inc_time`), terrain craters, farms' growth (`Farms::grow`, `snip`), deaths (`CheckSums::check_deaths` 0x00936BB0 sums them, so corpse timing is checked), `Guy+0x1C` (previous angle), diplomacy (`LeaderData::is_ally` 0x006EDB50), rules.xml Constants.
- **Drawing-only** (safe): tone mapper, camera shake, underlays, particles (`NukeOut` globals written after `init`), sprites, `FarmsOut`, facing interpolation, fog ramps, interface. The game's sound uses its own random generator (`SoundGlobal::random`), separate from the simulation's `game_random`.
- Mod rule: simulation-side mod code (shove.c, follow.c, nukefx radiation and interceptors, attrition, nonukes) uses only the frame clock and game state, never randomness, and the same setting for all players. The settings travel through `Game+0x20` (§ 6.3).

(CS § Nuclear weapons, § Dead units, § Units making way, § Unit facing; RB § Farms)

### 6.2 Commands: the only safe way to act for a player
- Human orders are commands. The UI issues them (`CommandManager::issue_spell` 0x00941B80, `GroupOut::issue_attack` 0x0070B060, `issue_build`, `issue_flight` 0x00708B10, ...). On every machine `CommandPackage::process` 0x0094A700 switches on the first byte (jump table 0x0094B5F4, 0x52 entries) and calls `Group::action_*`. **So game-rule hooks belong in `Group::action_*`, not in the UI.** (PD § Airdrop; CS § How the computer commands units)
- Command 0 is a group: `process_group` 0x0094A0C0 puts the units in a persistent group whose index the next unit order uses. Order codes: 2 stance, 3 form, 4 attack, 5 siege attack, 6 swarm around, 7 move to, 8 move near, 9 attack ground, 0xA patrol, 0xC halt, 0xD transport, 0x10 repair, 0x11 trade, 0x13 gather, 0x14 garrison, 0x15 disband, 0x16 gather point, 0x17 spell, 0x18 queue up, 0x19 build, 0x1C flight, 0x1E follow, 0x1F guard, 0x21 build mask, 0x23 recall, 0x24 scramble, 0x25-0x2F diplomacy and market, 0x44 chat (with its taunt number), 0x48 camera.
- `CommandPackage` 0x00E8FF88 (inside `command_manager` 0x00E8FF60) holds **0x200 bytes** (used size at word +0x10). **A command that does not fit is dropped silently**: spread big batches over frames. (CS § Airbase commands, § Right-drag)
- **The computer never sends commands**: it calls `Group::action_*` directly from its armies (§ 9).

### 6.3 Game settings and multiplayer flags
- `Game+0x20` is the setup's flags dword: bits 0..11 = the check boxes (rules.xml `gameinfo_flags`; bit 2 nation powers off, bit 4 Cooperative AI, bit 7 Tournament, bit 8 Lock Game Speed, bit 11 Disable Saving), bit 31 for map style 7. **No code reads bits 12..30**, so mods store settings there: bit 14 = Nukes None, bits 15..21 = the Nukes slider stop. The lobby sends the whole dword ("lobby_flags", `ConnectionData+0x2BE`), clients apply only bits 0..10 to their boxes, and it is saved with the game (`GameInfo::walk_data` 0x005D6570). `SetupWin::reset_game` leaves bits 12+ alone; `ScenarioRead::import` clears the dword. (CS § No Nukes check box, § Nukes slider)
- `game+0x820`: `& 4` multiplayer, `& 0x10` replay (`issue_attack` does nothing then), `& 0x42` show everything. `game+0x2D` = map / game style; `Game+0x24` = team style (8/9/10 Cooperative Teams, `Game::is_cooperative` 0x00594810). `game+0x544` set while loading a saved game. Globals: `GameAccess::game` 0x00C061EC, `gamec` 0x00C061E8. (CS; DC § More findings)

### 6.4 Saves and replays
- `SaveGame::do_save` 0x005A81F0 / `LoadGame::do_load` 0x005A6E30 drive `WalkDataGame::walk_data` 0x005A2360 (walker `+4` = 0 save, 1 load). Objects save fixed byte ranges: `SubObject` +0x8..+0x18, `Object` +0x20..+0x42, `Unit` +0x48..+0xB7, then path stack, orders, Guys. **No free bits remain in `Unit +0x48..+0xB7`.**
- Mod state the game does not know needs a carrier. A sidecar file does not travel with a save. `ScenarioData::temp_save` 0x00E88AFC is a String saved in every battle save and never used: nukefx.c writes its tables into it for the save call only, restores its value at once, and reads them back on load. This travels to other players (SyncFile) and into replays.
- Replays only say "Random" for the nation; the log line tells the real one.

(CS § Veteran ranks, § Nuclear weapons; DC § Saved games)

---

## 7. Players, leaders and the economy

- **Leaders** at 0x00E3A390, **0x6EEC bytes** each, index = player 0-7 (local player = `[*0x00C06210 + 0x298]`). `[leader+0]` flags: bit 0 exists, 2 (0x4) human, 3 (0x8) Cooperative AI share, 0x40 defeated. `[leader+4]` scenario bits: 1 no city defeat, 2 no unit AI, 4 no production AI, 8 no combat AI, 0x10 no city AI. Fields: +8 index, +0xC tribe, +0x7E4 pop cap, +0x940 current pop, +0x5222 + 2·type owned count, +0x5A22 + 2·type queued, +0x6928 team colour index, +0x6C18 obsolete-type mask, +0x6D94 nation-power bits, +0x6EB8 the encoded block (§ 1). Workers' places +0x8A4.. (food, timber, -, knowledge, metal, oil). (DC § Players; CS § Who plays a nation)
- Switching human/computer: console `computer <name>` / `human <name>`, or `ScenarioFuncSet::change_to_ai` 0x009E5D20 / `change_to_human` 0x009E5C80 (player from 1). It flips leader bit 2 and every unit's `+0x68 & 0x40000`. Local only: it would desync in multiplayer. (CS § Who plays a nation)
- **Pop cap** `Leader::calc_pop_cap` 0x006DC490 clamps to the game's limit (e.g. 200) in the middle, then adds the tribe bonus and wonder 0x20F, so a bonus added after it goes past the limit. **Resource caps** `calc_resource_caps` 0x006CE900: base by commerce level + bonuses + `[this+0x918+4r]`, `min(999)`, knowledge always 999; 999 is also hard-coded in `draw_cap`, the Dutch interest path and `Options::describe`. **Gather** `calc_gather` 0x006CEEE0 recomputes about every 256 ticks; `do_gather` 0x006CE450 uses min(rate, cap); the treasury gets rate / GATHER_RATE (450) every frame. Dutch interest: rate + excess x 5 %, capped at cap + 50. (DC)
- **Costs:** `TypeData::get_cost(r, player, -1, -1, 0, 1, -1)` (vtable +0x78) adds the player's price increases to the base. `can_pay_cost` 0x00667570 subtracts the AI's reservations (`leader + 0x468 + 4r`). (CS § AI production)
- **Cities:** a building's city is found only at build time and at `City::close` (`Build::find_city` 0x00628180). `get_town` 0x00639B90 needs tile flag 0x100 (inside a city circle, `Wall::mask_city`), then the nearest city within its radius. City radius = 20 + (level-1)·4 (+4 with tribe bonus 0x15), at most 64. Distance: `vector_dist = max + min²/(2·max)`. Capital = `City+4 & 0x10`. Borders are a separate 4x4-tile territory grid (`World::compute_all_territory` 0x006B5700); `WorldData::get_whose` gives a tile's owner. (DC § Which city; CS § Kills of a multiple selection)
- **Placement:** `BuildTypeData::blocked_site` 0x00636A50 returns 0 for a good site or a message code (0x18 "Can't build in enemy territory", ...). Its callers are the cursor, `Group::action_build`, the AI and walls. (DC § More findings)
- **Repair:** `Unit::do_repair` 0x005EE420 charges resources per hit point (formula in DC § City repair). Cities other than Large / Major heal by themselves (CITY_HEAL_RATE).
- **Attrition:** `Unit::process_attrition` 0x005E11A0 sets the period `[unit+0x9E]` = ATTRITION (48) x `get_attrition(owner)`/256. Forage / Supply / Logistics make it slower; it is halved for citizens and siege, and lowered per age of difference. Peace and assassin cases are 8 frames. `suffer_attrition` 0x005E1A10 hits through `take_damage` with nobody credited. (CS § Attrition)
- **Armageddon:** `Game::get_armageddon` 0x00594020 = ARMAGEDDON + per nation x nations + per team x teams, scaled by game style (x3 for 7, x2 for 5/6/0xB/0xC, at least 100 for 8). The formula is inlined in five more places, so mods change the Constants, not the function. `game+0x6E0` counts detonations; `GameDaemon::process_victory` 0x00730EF0 defeats everyone at the limit. (CS § Armageddon, § Nukes slider)

---

## 8. Nations (tribes)

- **24 nations are compiled in.** `Tribes` 0x00E7FA20 (`ObjectArray<Tribe>`, 0x5F0-byte entries); slot 24 = Random. `enum TribeIndex`: AZTECS 0 … PERSIANS 23, then RANDOM 24, OFFENSIVE 25, DEFENSIVE 26, ECONOMIC 27, XPACK 28. `Tribes::init` 0x006EF430 refuses any count but 24. Tribe fields: +0x18 name, +0x54 power tribe, +0x60/+0x64/+0x68 backup build / build / unit style, +0x70 the unit-type map (0x160 ints). (RO § 24 nations)
- **Adding a 25th** (Romania = tribe 24): the constructor and loop ends, `Game::init_tribes` 0x0058A600 (`% 24` and themed jump tables), setup combos and tooltips, `PlayerProfile::load`, and stride loops (`walk_rules_data`, `GameReportWin`, `UnitBalance::init`). Every TRIBE_MASK needs a 25th character. GRAFT handling (`UnitType::init` loop unrolled 4x6 = 24, `Types::finalize_grafting` 0x00669840) skipped tribe 24 silently, as did icons (`cmp eax, 0x18` at 0x0090D4F1) and the music's tribe switch. The scenario editor, quick match and Conquer the World stay at 24. (RO)
- **Availability:** `1 << tribe & mask` (`LeaderData::tribe_can_type` 0x006D9380). `LeaderData::type_eligible` 0x006DBD10 (= 0 hides a button) and `type_avail` 0x006E33A0 (< 4 = disabled) gate everything: buttons, AI planners, silo launches.
- **Nation powers:** all go through `LeaderData::has_tribe_bonus(tribe)` 0x006E1370 (0 if powers are off, `[gamec+0x20] & 4`; else the bit array at leader +0x6D94, else `power_tribe == tribe`). About 320 call sites, each one piece of a power. Help text is `TRIBE<power tribe>`. (RO § Nation power)
- **Unique units** name their generic in `<GRAFT>`; the same `<NAME>` means the same unit in other art. Per-nation aircraft are extra unitrules entries with the same NAME, GRAFT = that name, their own mask and GRAPH. No new types are possible, so Romania's units live in campaign slots (335..349). (RO § Units of a nation's own; RB § BH3)
- **Building styles:** BUILD_CONTINENT 0 Med, 1 NEuro, 2 Asian, 3 Korean, 4 American, 5 Turk, 6 Iroquois, 7 Colonial, 8 EIndian. Ages 1-3 have only four sets (`_0m`, `_0s`, `_0a`, `_0i`). Age 4 differs most. From age 6, 13 of 17 textures are shared by all; ages 5 and 7 entries are identical for all. UNIT_CONTINENT (0-5) is separate. Per-nation art is swapped by the DLL (`nation_art.c`), keyed on the tribe that draws the sprite. (RB § Building styles)
- **Loading screen:** `LoadingWin::prepare_unique_list` 0x00836830 lists units flagged `y`; the layout has 8 places and the 9th crashes (`on_mouse_move` has no NULL check, 0x00836BA6). It is drawn before `update_all_icons`. (RO § Loading screen)

---

## 9. Objects

### 9.1 The object table
- `Objects *` = `*0x00C0618C`. Per player p, `[objs + 0x14 + 0x1C·p]` points to the object pointers (also `[0x00C0AB84 + p·0x1C]`). **Ids:** units 0..`[+0x15C+4p]`-1, buildings 2000..`[+0x184+4p]`-1. A slot is in use when `byte +8` bit 0 is set. (PD § Airdrop; CS § Hover highlight)
- **Object fields:** +9 owner (byte), +0xA id (short), +0xC/+0x10/+0x14 z, x, y (`^0x63637`), +0x18 TypeData*, +0x20 max hits, +0x24 damage taken, +0x28 inside-list link, +0x3B damage sixteenths, +0x3C LOS.
- **Unit fields:** +0x50 angle, +0x68 flags (bit 0 decoy, 0x8000 Forced March, 0x40000 AI-controlled unit, 0x1000000 unit AI off, 0x2000000 entrenched), +0x6C state (bits 20-31 free in the original: ranks.c keeps experience there), +0x80 move/selection group (short), +0x82/+0xB4 previous in the building's inside list, +0x86 HeroData index, +0x8A/+0xB3 collision blocker, +0x8E captain (bit 15) or the captain's id, +0x90 next soldier, +0x96 mana spent / age, +0x9E attrition period, +0xAA formation, +0xAB spread, +0xB8 path stack (`Stack<PathData>`, 16 bytes each, top = next waypoint), +0xC8 / +0xDC orders, +0xE8 / +0xF4 Guys (the drawn soldiers). (CS § Veteran ranks, § Chasing, § Zero-length animations)
- **vtables:** UnitData 0x00B41B08 (+0x18 is_unit, +0x1C is_building, +0xC4 is_hero, +0xCC is_supply, +0xD0 is_caravan, +0x120 attack, +0x128 los, +0x138 valid_target_const). Object vtable: +0x94 change owner (gives a new id: a bribed unit is a new object), +0x114 hits_left, +0x11C max hits, +0x158 die, +0x168 repair_damage, +0x16C take_damage, +0x188 is_under_attack, +0x18C construct_time. Buildings are `Build` objects (vtable 0x00B42174), cities and wonders too. (CS; DC § City repair)
- **Making units:** `Objects::init_unit` 0x0065E0C0 finds a free spot itself and **ignores population**; `Build::finished` 0x00628490 checks pop / caravan / aircraft limits and calls `Build::train` 0x0062F9B0. A squad counts once (`Unit::set_type`); `check_population` 0x006E1330 returns 1 when there is no room. `ObjectData::is_active_build` 0x0046FB40 tells a finished building from a construction site. (DC § Training; PD)
- **Production queue:** count `byte [b+0x82]`, `BuildQueue` at `b+0x88` (0x14 per item, type at +4). **A finished item's slot keeps its old type**: read the count first. Progress = `get_job_counter` / `train_time` 0x006508C0. `Build::queue_up(type, 0)` 0x00620F40 queues and pays. (CS § Production bar, § nukefx.c)

### 9.2 Squads and groups
- A squad is a chain of unit objects: the captain (`+0x8E` bit 15) and soldiers linked by `+0x90`. Each soldier is its own object with its own damage; the captain's bar measures the squad's loss (`UnitData::total_damage` 0x00609C00). Soldiers per squad = `UBER_SIZE` (`curr_uber_size` 0x0060A760). (CS § Hover highlight; EN § Squads)
- **Persistent groups** `[0x00E85F20]`, 0x9D4 bytes each: +8 army index, +0xC count, +0x4A player, +0x8CC ids. **Selection groups:** the local selection is `[0x00E8D444] + [console+0x2A0]·0x9F0`, ids at +0x8CC, count +0xC; `Group::add` 0x00714350 caps at **128** (about 320 code sites read the arrays, so the cap cannot just be raised). `Group::kill` 0x00714110 takes a unit out with its whole squad. `GroupData::member` 0x0070F8F0. (CS § How the computer commands units, § Hover highlight)

### 9.3 Orders, movement, collisions, formations
- `Unit::do_job` 0x00617A10 runs the order kinds: 1 Move, 2 AttackTo, 3 ExploreTo, 4 FleeTo, 6 Build, 7 Gather, 0xA Attack, 0xB Follow, 0xC Guard, 0xD Repair, 0xE Cast, 0x12 Form, 0x13 GroupMove, 0x1A Garrison, 0x1B Think. `Unit::work` 0x0060D180 checks paths and targets every 16 frames (`(frame + id) & 15`) and repaths guards every 64. `Unit::go_to` 0x005F7A50 is a normal move; `set_new_location` 0x005F8D20 teleports exactly. (CS § Chasing; EN § Moving a unit)
- **Collisions:** `detect_unit_collision` 0x00617060 stores the blocker; `resolve_unit_collision` 0x005F9D30 attacks enemies, waits for moving blockers, walks around idle ones (at most 16 local paths per player per frame). **The game never moves the blocker** (shove.c does: make way, and own / allied units pass through). (CS § Units making way, § Passing through)
- **Chasing:** casters walk to a spot near the target's *old* position and never repath (follow.c `cast_follow`); Follow keeps 0x180..0x600 from the target; guards repath only every 64 frames (`escort_follow`). (CS § Chasing a moving cast target)
- **Formations:** `Group::action_move_near` 0x00704990 → `compute_form` 0x00707C80 → `compute_rows_and_columns` 0x0072D910. Row formations give every category over 5 units exactly 2 rows; formation 9 (Mob) gives each unit its own move. formation.c: mixed groups → Mob, fronts → blocks. (CS § Formations)
- **Facing:** `Guy` +0x18 angle, +0x1C previous, +0x64 target; `GuyOut::draw` quantised to 256 facings through `fast_angle_to_degrees` (facing.c draws the exact angle). Aircraft write both angles (`set_angle(a,1)`), which kills interpolation. (CS § Unit facing)
- **Animations:** `Guy::inc_time` 0x005D9E10 loops `while (time >= length) set_anim(...)`; a listed animation of loaded length 0 hung the game for ever (seen on an AI Artillery). animfix.c turns a 0 length into 3. (CS § Zero-length animations)
- **Rally points** are gather points (`BuildData+0xC8` count, `+0xCC` list; x/y at +4/+8). (CS § Rally line)

### 9.4 Combat
- `Object::do_damage` 0x0064A480 (this = attacker; returns early for a decoy; divides by the **attacker's** UBER_SIZE) → `get_damage` 0x00644130 → victim `take_damage` 0x00652020 (vtable +0x16C, 9 args, `ret 0x24`: a1 hits, a2 sixteenths, a5 = 1 quiet, a7/a8 attacker or -1). The victim dies when `+0x24` reaches max hits. Splash: `Ammo::do_damage` 0x00678060 (argument 7 = 0 for the shot's own target), plus an eighth of a hit spread by `do_damage`. (CS § Damage like attrition, § Decoys; EN § Second test)
- **Counters:** `Balance::type_damage` 0x0057FB50 is a hard-coded table (start at 100, multiply per matching rule); `compute_modifier` 0x00581CC0 adds balance.xml and caches a 493x493 table at 0x00C06AFC. Siege 520 % vs buildings, Flamethrower x2.5, Riflemen 66 %, Machine Gun 50 %, Bowmen / Knights 33 %. (CB)
- `max_range` 0x0060FE90 = `+0x1FC` + bonuses; recharge `+0x1F4`, slower out of supply. Health colour thresholds 90/75/50/25 % (`health_level` 0x006534B0). `Unit::update_hits` 0x0060E930: the captain adds bonuses and copies to every soldier. (EN § Combat values; CS)
- **Entrench** (the General's ability): flag `+0x68 & 0x2000000`. Hits from the front (about 60°) take x2/3, except from Flamethrowers (0x83). It ends on a move, a flamethrower hit or death. The dirt mound comes from `GraphicEvents::add_entrench` 0x008E4670. (EN § The game's own entrench)
- **Garrison:** `Unit::do_garrison` 0x005E6B80 rechecks `can_garrison` 0x0061D7D0 **every frame**: land units only into the City line, the Fort line and their own production building; ships only into Docks. (EN § Garrison)

### 9.5 Crafts (abilities), Generals, decoys, ranks
- `Options::do_spell` → `CommandManager::issue_spell` → `Group::action_spell` 0x006FE1A0 → `Unit::do_cast` 0x005EBFE0 (target at order +8/+0xC, craft at +0x20). `SpellType::cast` 0x00676CE0 needs `is_castable` == 3 and jumps on type - 0x275. Rally (0x279) exits empty (`cast_rally` is `ret 8`): our General's Heal hooks it. An area craft (flag d) has target -1 and `do_cast` crashed on it (0x5EC14E): Paradrop catches 0x284 first. Untargeted crafts (no b/c/d) cast at once. Mana: `SpellType+0x1D0` cost, refilled by `Constants+0xA0` per tick. (CS § The General's Heal; PD § Test)
- Caster lookup: `get_spellcaster` 0x0070CA10 (-1 silent) vs `get_valid_spellcaster` 0x0070C770 (-1 = red feedback + sound 0x40); `describe` shows the price only when it finds a caster. (PD; CS § Dimmed abilities)
- General: `HeroData::get_radius` 0x00739E50 (tiles); HeroData at `[[0x00C0A130+0x1C·p]][word general+0x86]`. Decoys deal no damage; life = DECOY_TIME x (upgrade+2)/2 - age. (CS § The General's Heal, § Decoys)
- Ranks (ranks.c): XP per hit = victim cost x 256 / (victim hits x attacker cost), with a fractional remainder; cap 0xFFF (level 10). Building experience in Build +0x65 (unused by buildings). (CS § Veteran ranks)
- Bribe key: Bribe is bound in the Spy's key group, so a Senator had no key: heal.c presses it. (CS § The Senator's Bribe key)

### 9.6 Aircraft, missiles and nukes
- Airbase row: Scramble 0x2B, Repeat 0x2C, Recall 0x2E, Rally 0x33; `Object::do_launch` 0x0064F3B0 launches one aircraft per FRAMES_BETWEEN_LAUNCHES (0x00C06248, 15). A flying model sits at least 0x96 above the ground; drawing uses the height written after `Objects::process_all`, not `get_render_z`. (CS § Airbase commands; PD)
- A silo makes one missile at a time (`num_aircraft_limit` 1; `can_carry` refuses while one is queued or parked). A launched missile fires an **Ammo** and dies. The flight is the Ammo (`objects+0x11C` array, start/end, cur/total time, spline), not an object, so anti-air can never target it. `Ammo::close()` 0x006791A0 frees it with no blast. (CS § Nukes in flight)
- The blast: `Nuke::add_nuke` (only caller 0x00678516) records x, y, damage radii (100, then SPLASH x 192), start, player. Damage runs from start+10 to +40, removal at +430 frames, and the Armageddon count goes up at age 110. The crater (`terraform_for_nuke`) stays all game. The screen flash is the tone mapper written by `Nuke::draw` 0x0092B370; the camera shakes for every blast wherever it is. Missile Shield (type 0x2B5) cancels a nuke over its territory. (CS § Blasts, § The flash)

### 9.7 Deaths
`DeathObj::inc_time` 0x008D5240 ages a dead unit and removes it at anim + 135 frames (627 with corpse art). The fade spans are disp8 immediates (≤ 127). The pool is fixed at 100 (`Objects::init`; corpses.c raises it, `corpse_limit`); when full, `add_death` reuses the oldest non-blocking one. Tank wrecks block until `clear_blocking` 0x008D4AC0; calling that early removes the object twice. Buildings have no death art: they sink (`ObjectsOut::pop_all_zoomed_in` 0x00655E70) with particles. (CS § Dead units; RB § How a destroyed building falls)

---

## 10. The computer AI

- **Planning:** `Leader::plan_strategy` 0x006B9620 → `production_ai` 0x006C1960, a step machine (`[leader+0x788]`): setup, `found_cities`, `research_techs`, `upgrade_units`, `create_units`, `create_buildings`, `make_stuff`. Every step offers candidates through `MakeList::make_me` 0x006C9BE0 (list at `leader+0x6EC8`, keeps the best few); filtering there by return address is how mods steer the AI (Generals and Supply Wagons in proportion to the army; no Interceptor offers). `create_buildings` asks `type_avail`, so answering 0 from there keeps a building from the AI. `create_units` divides by POP. (CS § AI production; DC § Crash found)
- **Armies:** `Unit::add_to_army` 0x005F7740 / `Army::add_unit` 0x006F9F40 take units in; every army order goes to `Group::action_move_to` 0x0070FBA0, `action_siege_attack_to`, `action_stance`, `action_halt` from listed call sites. Builders: `produce_building` places the foundation and sends Citizens with `action_swarm_around` (which orders only Citizens 0x32/0x33 and casters of craft 0x293). (CS § How the computer commands units)
- **A unit's own AI:** `Unit::think` 0x005F6E40 (from `do_idle` when idle, and periodic think orders). The computer's part runs when the leader is not human, or for a unit with `+0x68 & 0x40000`. **Cooperative AI** (box 4) sets leader bit 3: the leader stays human, but the planners and armies run for it and new units get 0x40000. assist_orders.c keeps units you ordered away from the computer's armies until they idle for a while. (CS § How the computer commands units)
- Silo launches by the AI: `Object::do_launch` 0x0064FDF4.., limited to `get_armageddon() - 2`. Talking to computer allies: a typed taunt number 1-100, or signals; where the AI reads them was not found. AI names: un-met enemies are hidden on purpose (`StatWin::setup_normal_entries`, `LoadingWin::draw_other_player_info`); loading a save skips the naming loop. (CS; DC § More findings)

---

## 11. The user interface

- **Windows:** `IFaceButton` ctor 0x007FC190 + `Button::init` 0x00524E60; `Window::show` 0x00A55B30, `hide` 0x00A54780, `move` 0x00A54080 (parent coordinates), `bring_to_top` 0x00A53F90, `is_visible` 0x00A54740 (`[win+8] & 1`), `+0x74` parent, `+0x126` rect, vtable +0x20 resize. The Console (map window) is `*0x00C06210`, IFaceMain `*0x00E37B9C`, `iface_current` 0x00E39964 (`+0x470` = 0 while folded). `Window::recurse_clips` 0x00A50070 clips children to their parent. (CS § Custom in-game window; UI)
- **Drawing interface quads:** `IFaceRenderManager` global 0x00EBD1C0; `add` 0x0051F3D0 (tex NULL + type 5 = solid rectangle). `flush_all` 0x0051FBE0 draws in texture-bucket order, not call order, so a solid fill can hide a texture added after it, and text over a solid rect gets a halo. Text: the window Buffer (+0x154, font +0x7C), `write_l` 0x0050F6E0; fonts `FontFamily::get` 0x00522810 (Arial, Georgia, Courier: Latin Extended and Cyrillic, no CJK). (CS § Resource panel internals, § Game setup screen combos)
- **Tooltips:** `MouseOver::set_help` takes a help.xml key; `MouseOver::describe` → `Options::describe` 0x00724460 writes the title to `Scene+0xDC` and lines to `Scene+0x104` (`Scene *` = `*0x00C0620C`); extra lines only at help level ≥ 2; drawn by `Scene::render_mouseover_text` 0x008BDA80. Costs: `Scene::set_good_required` 0x008BE090. (CS; DC § City repair)
- **Command panel:** `Options::add(code, col, row, type, disabled, 0)` 0x0072C290 on `*0x00C06204` (0x14 per entry, at most 0x64). Codes: 0..2 build, 3 repair, 4 craft, 0x1A garrison, 0x1F eject, 0x22 launch, 0x24 Air Mission. `BuildOut::make_options` 0x00629ED0 / `UnitOut::make_options` 0x00615FC0 fill it; `IFaceButton::on_redraw` 0x007FC730 draws icons. (PD; CS § Airbase commands)
- **Mouse:** `Console::execute_at_cursor(cursor_only, mode)` 0x007C6630 does both the hover cursor and the right-click order; `on_right_up` 0x007CD3C0 issues drag orders. MouseOver `*0x00C06230` (+4 bit 4 over an object, +0x58 id, +0x4C player). (CS § Right-click crafts, § Hover highlight)
- **Keys:** `Console::on_key_down` 0x007CC210 (VK + 0x10000 Shift / 0x20000 Ctrl / 0x40000 Alt) → `KeyMap::lookup` 0x007D4780; `keymap_options` 0x00AE7770 with key groups (16 any unit, 64 Spy, 128 General). (CS § Hotkeys)
- **Selection panel:** `IFaceSelected::update_selected` 0x00823790; with several objects selected only `draw_group_name` runs; `IFaceMain::position_windows` 0x00809250 places it (one object: resized, never moved). (CS § Selection panel while folded)
- **Resource panel:** `IFaceResources::on_redraw` 0x0081B3B0, state byte +0x470 (market / resources / workers open), rects at +0x4A4, `adjust_rects` 0x00819A90. The pop / city counters look their rects up by name every draw. (CS § Resource panel)
- **Messages:** the log is `MessageWin` (`add_message` 0x007E9FB0). Centre announcements are `AnimNotice`s (`IFaceMainBase::do_notice`): only the first live one is drawn, so they queue. Clocks: `IFaceMainBase::draw_timers` 0x0080B060. (CS § Centre-screen announcements; SV)
- **Minimap:** `WorldMap::update_world_map` 0x008D2BA0 (rect at IFaceMainBase `+0x10BC`, `reinit_all` rebuilds). (CS § Minimap)
- **Setup screen:** `validate_start` 0x005BAC20 (team rules), check boxes `setup_checks` 0x005C16A0 / `on_check_clicked` 0x005C5920 (box ids above 10 have no bound check), combos at `SetupWin+0x20BC` (nation 8..15, colour 0x10..0x17). `SetupWin+0x2A6C` is an embedded object, not a pointer (reading it as one crashed). (CS § Start checks, § No Nukes check box, § Game setup screen combos)
- **Languages:** `LocalizationManager` supports 5 (limits are `cmp 4/5` in code); lang.c adds 10. `String(const wchar_t*)` 0x00A1EDD0 keeps the pointer without copying, so mod strings must stay alive. (CS § Languages)
- **High-DPI:** the 2D UI is laid out in physical pixels; the virtual-resolution scale is at `graphsys` +0x350..+0x364. (CS README)

---

## 12. Rendering

- **Camera:** **orthographic** (view direction (0,0,+1)). `Game::init_camera` 0x00590BC0 sets 7 zoom levels with tilt 60; distances at `Camera+0x320`; `zoom_out` stops at level 4 (2 in a replay), our patch at 0x00844001 lowers it to 3. `Camera+0x304/+0x308` = the screen centre on the ground, `+0x2F0` the zoom distance (18544 at level 5), `+0x294` the level. `Camera::set_loc_adjust` 0x008438C0 shakes it. `world_to_console` rounds to whole pixels, which is why buildings jittered. (CS § Zoom levels, § The flash)
- **Frame:** `Scene::render`: ground (`TerrainOut::render` 0x008558B0), underlays (`Underlays::draw`), buildings and units, `Nuke::draw`, then the tone mapper (`D3D11ToneMapper::do_run`, `shaders\ps_tonemap.hlsl`: +8 exposure, +0xC white flash, +0x1C saturation, ...), FXAA (`D3D11AntiAlias::run`, takes luma from alpha, which only bloom writes), overlays and interface. Shaders compile at run time from `shaders\*.hlsl` and can be overridden; constant buffer b7 is free. Depth is D32 with **no stencil** and no MSAA. (CS § Picture look, § The flash)
- **Ground:** TerrainVis (`*0x00C06218 + 0x533C`) streams blocks of 25 or 9 vertices per tile with **unsigned short** indices, so a stream past 2621 blocks wraps and draws streaks (`ground_guard` caps the zoom and clears with `set_zoom(5)` 0x008B3190 + `clear` 0x008B1480). The visible ground is rebuilt when `camera+0x28C` (or scene +0x22C / +0x229) is set. A stale set shows as black ground with a slanted edge. (CS § Ground vertex streams, § Fog of war)
- **Fog of war:** `TerrainVis::fill_fog_texcoords` 0x008B19F0 reads the fog grid `[0xE8A894]` and looks up a 256x1 ramp (`fow_fade.tga`); a cliff in the ramp gives sawtooth edges (fog.c blurs it). Fog affects only the ground drawing, not which objects show. (CS § Fog of war)
- **Water:** quality picks Normal / Min Water (`TerrainOut::render_water` 0x00852CD0). Reflections reuse the previous frame's scene and depth; water and trees write no depth, so only buildings and units can be reflected. (CS § Water)
- **Units:** `UnitOut::draw` 0x00615B80 depth-sorts the soldiers; `GuyOut::draw` 0x005DC490 → `GraphicPieces::draw_unit` 0x008E7DA0; team colour through TFACTOR. Team colours: `team_colors` 0x00E39CE0 + 0x64 x leader+0x6928 (`TeamColor::get_unit` 0x008C2DB0). (CS § Hover highlight)
- **Overlays:** selection circles `draw_unit_circles` 0x00606A10, `draw_circle` 0x008C6040 (batched as RenderBatchType 5: **past 16383 vertices d3dgl hits int3**, so long rally lines crashed; rally.c flushes above 8000), health bars `UnitOut::draw_health_bar` 0x00613810 (captains, selected or hovered) and `GraphicPieces::draw_health_bar` 0x00908650, building rings `BuildOut::draw_underlays` 0x0062BDF0, cursor follows `Options::draw_cursor_follows` 0x00723C00. (CS § Rally line, § Range circles; PD)
- **Underlays:** flat textured quads on the ground from effects_graphics.xml `<UNDERLAYS>`; instances in `[0x00C8C720][type]` (`ObjectArray<UnderlayStruct>::add` 0x004A9D20; 0x20-byte struct: x, y, z, angle, remaining ms, total ms, width). `Underlays::add` adds only points the local player sees. (CS § Underlays)
- **Effects:** `GraphicPieces::add_explosion` 0x00904CC0, `add_pulsewave` 0x00904DD0 (no game random; drawn only on seen tiles); particle systems from effects_graphics.xml.
- **Farms:** no sprite: a terrain underlay plus stalks generated by `FarmsOut::draw` 0x008D75F0 from `<FARM>` and the tileset's `<FARMTEX>`. Change only `FarmsOut` to stay in sync. (RB § Farms)
- **Placement ghost:** `BuildTypeOut::draw` 0x00634060 (also draws a falling building). (DC § Expansions; RB)

---

## 13. Sound and music

- `xasl.dll` plays everything through XAudio2 2.9; the exe imports no audio API. `SoundGlobal::play(cat)` 0x0097F770 is called only for the local player. Its enum order comes from `sound_global_cat_names` 0x00ED3568, not soundglobals.xml (GENERAL_AMBUSH 0x2C, GENERAL_MARCH 0x2F, GENERAL_RALLY 0x31; error feedback 0x40; check box 0x111; refused 0x0F; Missile Shield 0x9A). Unit sounds go by graphics FAMILY in soundtypes.xml; new wavs are appended to soundfiles.xml. (CS § The General's Heal; NM)
- **Music:** `JukeBox::init_game` 0x0097DDC0 reads `Data\sound.xml` `<TRACKS>`: a `<TRIBE tribe="name">` block matching the local nation, else "default"; inside, `<AGE age=0..7>` and `<MOOD>` (ECONOMIC, BATTLE_VICTORY, BATTLE_DEFEAT). `shuffle` steps down to the nearest earlier age with tracks. `set_next_mood` ignores changes within 30 s of a track's start; battle moods cut in at once (score ratio above 4/3 or below 3/4, a lost city, scripts), ECONOMIC waits for the track to end. (RO § Music)
- **Memory:** `add_track` opens every player's tribe tracks, and sound.xml repeats tracks, so many big ADPCM tracks can exhaust the 2 GB address space (model loads then fail: "art\hammer5.bh3 does not exist"). (NM § Crash at game start)

---

## 14. Lessons: things that broke and why

1. **POP 0** on a unit: divide by zero in the AI (0x006C4BFA). Always POP ≥ 1.
2. **Reordering craftrules.xml** breaks crafts: the code knows them only by position.
3. **An area craft** (flag d) crashed `do_cast` (target -1): the path was never finished in the game.
4. **A file only in mod_override** was invisible to folder searches until `FindFirstFileExW` was redirected.
5. **English-only overrides** vanish in other languages: every changed XML needs its `.N` copies (translations/).
6. **A 9th unique unit** crashes the loading screen; **a 25th nation** is silently skipped by every loop unrolled to 24.
7. **A zero-length animation** hangs the game for ever (animfix.c).
8. **Too many vertices:** depth meshes over the Pre-Transform budget are not drawn; over 16383 circle vertices d3dgl hits int3; ground streams past 2621 blocks wrap.
9. **Winding:** depth meshes must face away in raw .bh3 coordinates, or walls show vertical strips.
10. **Command package full** = commands dropped silently.
11. **`can_garrison` runs every frame:** any order it does not like is dropped at once (the first Nest test).
12. **The production queue keeps finished items:** read the count byte.
13. **Plain text in `set_help`** shows an empty tooltip: it needs a help.xml key.
14. **Interface draw order is by texture bucket**, not by call: solid fills and textures in one window can hide each other.
15. **Corpse fades** are disp8 immediates (≤ 127), and calling `clear_blocking` early removes the object twice.
16. **FXAA did nothing with bloom off** (it reads luma from alpha).
17. **`SetupWin+0x2A6C` is embedded**, not a pointer.
18. **A bribed unit gets a new id**: tables keyed by id must follow it.
19. **The setup's high flag bits travel but nobody applies them**: mods must read `Game+0x20` themselves on every machine.
20. **Truncating integer maths** lost XP from hits on buildings and made sprites step a pixel: keep fractions.
21. **A test DLL that waits in DllMain** was removed by the PC's security software.
22. `near` and `far` are macros in the Windows headers: not usable as variable names.

## 15. Open questions

- Where the AI reads typed taunt numbers and signals.
- `halfoffset`'s exact rule; whether art bigger than the original is drawn from its top-left corner.
- Which model gave the zero-length Artillery animation.
- The writer of `SetupWin+0x1F28` (the exact team counts).
- The territory grid (`World::compute_all_territory`) is not read yet.
- Whether the bottom of the path stack is always the end of the way (pass-through).
- The cause of black ground after a minimap jump.

---

## 16. Where the details are

| Topic | File |
|---|---|
| DLL features: camera, interface, AI, nukes, units, rendering, languages, launcher | [camera-smooth/ENGINE_NOTES.md](camera-smooth/ENGINE_NOTES.md) |
| Types, players, economy, cities, expansions, city repair, saves | [data-center/ENGINE_NOTES.md](data-center/ENGINE_NOTES.md) |
| Garrison, entrench, squads, building, test history | [entrenchment/ENGINE_NOTES.md](entrenchment/ENGINE_NOTES.md) |
| Crafts, unit creation, BH3/BHA, helicopters | [paradrop/ENGINE_NOTES.md](paradrop/ENGINE_NOTES.md) |
| Building art: depth meshes, projection, colour, light, styles, farms | [roman-buildings/ENGINE_NOTES.md](roman-buildings/ENGINE_NOTES.md) |
| Nations: the 24 limit, masks, powers, GRAFT, icons, music, models | [romania/ENGINE_NOTES.md](romania/ENGINE_NOTES.md) |
| Music formats and memory | [nation-music/ENGINE_NOTES.md](nation-music/ENGINE_NOTES.md) |
| Game scripts | [survival/ENGINE_NOTES.md](survival/ENGINE_NOTES.md) |
| Interface theme, launcher | [ui-theme/ENGINE_NOTES.md](ui-theme/ENGINE_NOTES.md) |
| Damage modifiers | [tools/COMBAT_NOTES.md](tools/COMBAT_NOTES.md) |
| Per-nation art state | [NATIONS.md](NATIONS.md) |
| Open ideas | [TODO.md](TODO.md) |
| What changed for players | [CHANGES.md](CHANGES.md) |
