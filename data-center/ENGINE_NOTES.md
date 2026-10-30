# Engine notes for the Data Center

What was found in `riseofnations.exe` (Steam Extended Edition, timestamp 6674863F, base 0x400000) while building the Data Center and its upgrades. Addresses are virtual addresses from `sbl/rise_z.map`; the game also ships `sbl/rise.pdb`.

Tools: `capstone` + `pefile` with the names from `rise_z.map` (a small disassembler script, `rdis.py <address|symbol>`).

## Types

- One global `TypeIndex` space. Units are `0x32 .. 0x19D` (unit *i* of `unitrules.xml` is type `0x32 + i`, 364 units). Buildings are `0x19E .. 0x21E` (129). Spells are `0x275 .. 0x2AB`.
- The exe refuses data files with a different number of buildings or techs than it was built with. New types are impossible, so existing slots must be repurposed.
- `typenames.xml`: the hash is `String::generate_hash` (0x00A1B6B0) of the internal name, so the shown text can change while the hash stays.
- `<WHERE>` in `unitrules.xml` is the building that trains the unit. A unit with `WHERE = Washington Monument` appears in the Data Center's panel.
- Campaign-only units have `WHERE None` and an all-zero `TRIBE_MASK` (indices 308-350: Alexander, Napoleon, ..., Junkers, Guerilla Fighters, Red Coats, Barbarian units, ...). They are the free slots for the Data Center upgrades. Conquest campaigns use some of them.

## Players

- `Leaders` array at 0x00E3A390, 0x6EEC bytes per player. `+8` = player index, `+0xC` = tribe.
- `+0x6EB8` = pointer to encrypted per-player values (each field XORed with its own key):
  - `+0x30 + 4*r` resource cap of resource r: `(cap << 4) ^ 0x1281`.
  - `+0x64 + 4*r` gather rate of resource r (x16): `^ 0x872`.
  - `+0xE8` / `+0xF0`: age-like values, `^ 0x63187`.
- `+0x7E4` = population cap (plain int). `+0x940` = current population.
- `+0x6C18` = bitmask of unit types that are obsolete or replaced.

## Population cap: `Leader::calc_pop_cap` 0x006DC490

Computes the cap into `[this+0x7E4]` and returns it. It clamps to the game's pop limit setting (`pop_limits` in the game options, e.g. 200) in the middle. After the clamp it only adds the tribe bonus and wonder 0x20F. A bonus added after the original function therefore goes above 200. It is called from `Build::finished` (new buildings) and from `calc_gather` when territory changes.

## Resource caps: `Leader::calc_resource_caps` 0x006CE900

For r = 0..6: base by commerce level, tribe and wonder bonuses, plus `[this + 0x918 + 4*r]` (per-player additive bonus). Then `min(x, 999)`, then `<< 4`, stored encrypted. Resource 3 is always 999. It is called every time from `Leader::gather` (0x006CE280), so a bonus added after it applies immediately.

## Gather rates: `Leader::calc_gather(int *out)` 0x006CEEE0

- Recomputes the six rates (x16) into `out` only every ~256 ticks per player (it checks `game time >= [this+0x7AC] + 300` and a per-player phase). Otherwise it returns at once and leaves `out` alone.
- `[this+0x7AC]` is set to the game time when it recomputes. A wrapper can compare it before and after to know whether the rates were recomputed.
- `Leader::gather` keeps the rates encrypted at `+0x64`, passes a decrypted copy to `calc_gather`, then calls `calc_resource_caps` and `do_gather`. `do_gather` uses `min(rate, cap)`.
- Scaling both the rate and the cap by k scales the real income by exactly k, even for capped resources.

## Training: `Build::finished(TypeIndex)` 0x00628490 (virtual, `ret 4`)

- Called when a queued job completes. For unit types it checks that `pop(+0x2F0) + current pop <= pop cap` (return 0 = wait), the caravan limit and the aircraft limit (return -1). Then it calls `Build::train(type)` (0x0062F9B0), which creates the unit with `Objects::init_unit` and returns its id. Then `finished` returns 1 (done).
- `this+9` = owner (byte), `this+0x18` = BuildTypeData*.
- A wrapper that returns 1 for an upgrade type without calling the original completes the job without creating a unit.

## Showing options

- `LeaderData::type_eligible(TypeIndex, int)` 0x006DBD10: `BuildOut::make_options` (0x00629ED0) skips a unit when it returns 0, so the button is hidden. `Build::activate` (the queue command) also checks it.
- `LeaderData::type_avail(TypeIndex, int)` 0x006E33A0: 4 = available. Calls `has_preq`, then `type_eligible`. Unavailable options that are still eligible are drawn disabled (our padlock).
- `BuildTypeData::queue_here(type, 0)` 0x00639D10: whether this building makes that type.

## Unit icons

`unit_graphics.xml` gives every unit `tex_id` (TEX_UNITS_0/1/2 = `art/iface_units_01/02/03.tga`, plus `_half` versions) and `tex_col` / `tex_row`. Repurposed units can get new icons by redrawing their own cells in overridden copies of those atlases.

## Saved games

- `SaveGame::save_game(const String &name, int)` 0x005A8220 and `LoadGame::load_game(const String &name)` 0x005A72F0 both open the file with `File::open(const String &, int)` 0x00A2DCD0.
- The state is written by `WalkDataGame::walk_data`. Mod state that the game does not know about needs its own file next to the save.
- `String`: wide characters. Data = `[s]` if `[s+0xA] & 1`, else `*[s]`; the text starts `[s+6]` characters in.
- New game: `Game::init_rules_and_teams` calls `Leaders::init` (0x006ED850).

## What the mod hooks (camera-smooth/src/datacenter.c)

Since 2026-10-26 the upgrades are gone: their bonuses come from the Data Center Expansions, 1x1 buildings. The bonus is (Expansions of a kind the player owns) x the step, counted by the game itself (`word [leader + 0x5222 + 2*type]`), so nothing has to be saved next to the game.

| Function | Why |
|---|---|
| `LeaderData::type_eligible` | Data Center: one at a time, one of the four last Library techs, AI income gate. Expansions: a finished Data Center |
| `Leader::calc_pop_cap` | Add the Population Expansions' bonus after the original |
| `Leader::calc_resource_caps` | Add the Commerce bonus, then scale by Efficiency; recompute the pop cap when the count of Population Expansions changed |
| `Leader::calc_gather` | Scale freshly computed rates by Efficiency |
| `Leaders::init` | New game: forget the last counts |

The first version (2026-10-24, removed 2026-10-26) also hooked `Build::finished`, `check_population`, `TypeData::get_cost`, `SaveGame::save_game` / `LoadGame::load_game` / `File::open` and `Options::do_cycle_research`; the notes below on those functions stay true.

## Expansions (found 2026-10-26)

- Free building slots: only El Dorado (building 34, type 0x1C0), the Washington Monument (35, 0x1C1, the Data Center), the Lenin Statue (36, 0x1C2, Entrenchment's Field Fortification) and the Courtyard Fountain (105, 0x207) are disabled scenario buildings with their own names. Buildings 37-104 are the scenario editor's Houses (36), Apartments (20) and Office Buildings (12): `PREQ0 disable`, garrison 5, in no city template (`citytemplates.xml` only places Farms and Libraries); skirmish cities draw their houses inside the city sprite. The exe mentions none of these type numbers in code (the immediates 0x1C0, 0x1C3, 0x1E6, 0x1E7, 0x1FB, 0x206, 0x207 only turn up in `Constants::log_data` and unrelated terrain code). Everything else in `buildingrules.xml` is used in skirmish.
- The Expansions: Population = El Dorado (0x1C0), Commerce = Courtyard Fountain (0x207), Efficiency = HOUSE35 (building 72, 0x1E6). HOUSE35 had the name "House" (shared by all 36 Houses) and the TYPENAME "House F5", which has no typenames string; it was renamed "Lighthouse".
- `typenames.xml`: the key is the building's `NAME`. `String::generate_hash(const wchar_t *, unsigned long *)` 0x00A1B6B0, for i from the last character down to 0, with k = i + 1, c the character, l = towlower(c) and P the 50 primes at 0x00B14500 (127, 811, 1597, ...): `h1 += P[i % 50] * c + P[c % 50] * k`, `h2 += P[i % 50] * l + P[l % 50] * k` (32-bit). It returns h1 (the `hash=` of typenames.xml; checked on El Dorado 4174297, House 969271, Courtyard Fountain, Washington Monument, Apartments) and stores h2. The BUILDS section has three strings no building uses: Bastille, Tower of London and Lighthouse; renaming a building to one of them gives it a shown name without adding strings.
- `masks.txt` (game root, read through the file overrides) holds the footprint masks named in `building_graphics.xml`: `1x1 solid` exists, no 1x1 doober mask did. `mod_override\masks.txt` adds `#1x1 doobers` (one cell, `up`). Not yet seen in game.
- `SUPPORT0/1` + `SUPPORTVALUE0/1` in `buildingrules.xml` are the ramp cost: each building of that type already owned adds that much of the resource (raw, not x10; `COST` is x10).
- The four last Library technologies are techs 35-38 of `techrules.xml` (types 0x243-0x246): Missile Shield, World Government, Global Prosperity, Artificial Intelligence; each needs its line's previous tech and the Information Age. `LeaderData::has_tech(type)` 0x006E0C80. `PREQ0..2` are all required, so "one of four" is done in `type_eligible`.
- `halfoffset` in `building_graphics.xml` (found 2026-10-26, after the Expansions were seen in game drawn about half a tile off their footprint): `GraphicPieces::init_build_data` reads it with `get_attrib_num` (default 0) right after `constructsize`, `spritexoff` and `spriteyoff`, and stores it at `[rdata + 0x98] + 0x2C` (`constructsize` at `+0x28`). `BuildTypeOut::draw` 0x00634060 (the placement ghost) subtracts `halfoffset * 96` from the world x and y before `world_to_console`; a tile is 192 world units (`coord >> 6` through `div_3_table`), so 1 moves the sprite half a tile on both axes. Also read by `GraphicPieces::init_position` (0x00901A06), `WallOut::draw_underlays`, `WallOut::draw` and `Doober::draw`. It is an art choice, not a rule of odd or even sizes: the game has 3x3 with 0 (15) and 1 (1), 4x4 with 0 (234) and 1 (54), 5x5 with 0 (144) and 1 (139). The Expansions' origin is the centre of their tile, so they need 0.
- The game's sun (fitted 2026-10-26 on tower_0m, tower_3m, lookout_0, tower_6, temple_0m, woodcutter_0: their depth meshes cast onto the ground against the alpha-145 shadow pixels of their sprites, best overlap 0.45-0.69): a point E * z pixels above the ground casts its shadow about (-0.78, -0.33) times that away on screen, left and a little up, towards the back. The ratio y / x (0.31-0.57, mean about 0.42) is steady; the length less so, the meshes being lower than the sprites. The game clips its own shadows at the canvas edge too (tower_0m). `make_sprite.project_shadow` with a fixed `SHADOW_AB = (0.55, 0.10)` (`make_nest.py`, and the Expansions until 2026-10-26) starts every column's shadow at the column's lowest pixel, the front edge, so it spread down-left; `make_sprite.py` itself fits (a, b) on each original's shadow. The Expansions now cast it from their geometry (`expansion_art.cast_shadow`: plinth and body as prisms) on a canvas widened to the left (`spritexoff` 39 -> 80).
- The ramp price (`SUPPORT0/1`) of the Expansions counts only buildings of the same type, so each kind gets dearer on its own.
- Seen in the same screenshot: the Expansion tower fills only about 0.63 of its tile's width (`make_expansion.py` assumes a 42-unit tile; the game's own meshes span about 55-62 units per tile). Not changed.
- Icon page `iface_buildings_wonders.tga` (TEX_BUILDINGS_3): 256x256, 4 x 6 cells. Free before the mod: (3, 2), (3, 3), (3, 4). The Efficiency Expansion uses (3, 4); the other two keep their slots' cells (0, 5) and (3, 5).

## The technician (found 2026-10-26)

- `Group::action_build(x0, y0, x1, y1, type, queue)` 0x00707510 first needs a group on the map that can build (`GroupData::is_on_map`, `validate_build`, `num_valid`), so a group of one building gets nothing. For every point of its path stack it then does: `LeaderData::type_avail(type, 1) == 4`; `BuildTypeData::snap_center(x, y, &x, &y, player)` 0x00636190; `BuildTypeData::blocked_site(x, y, player, -1, 0)` 0x00636A50 (0 = fine); `city = ObjectsData::find_city_at(x, y, player, -1, 0)` 0x0065B870 (-1 when the type's flags `[bt + 0x2C0]` have 0x2000 or 0x10); the price, unless the game is in a free-building mode: BuildType virtual +0x84 `(player, city, -1, 1)` says whether it can be paid, +0xD4 `(player, -1, city, 1, 0)` pays; `Objects::init_build(player, type, x, y, 0, -1)` 0x0065D190 lays the site and returns its id; `Group::action_swarm_around(id, player, queue, 6, 1)` sends the group to build it. The BuildType of a type is `[[0x00C061F4] + 0x10][type]` (indexed by TypeIndex). datacenter.c does the same for a Data Center, without the last step.
- `Unit::add_build_order(building id, player, QueuePos, int)` 0x005E5210: no unit-type check. It makes order 6 (`OrdersMemManager::get_obj(6)`); QueuePos 2 closes the unit's orders first (replace).
- `Build::train(type)` 0x0062F9B0 (`ret 4`): `Objects::init_unit(player, type, the building's x, y, -1, -1, -1)`, then sends the unit to the building's gather point. The technician comes out this way.
- `UnitData::is_idle()` 0x0046FA40, `Unit::go_to(x, y, 2, 0, 0x300)` 0x005F7A50, `Object::disband(0)` 0x006455C0.
- The technician is unit 341 (type 0x32 + 341), the Citizen's rules copied over a campaign-only slot (build_data.py `TECH_UNIT`); the 15 slots of the old upgrades are free again except this one.
- The hooks are shared: entrench.c's `Options::add`, `GroupData::count`, `LeaderData::type_avail` and `Group::action_build`, airdrop.c's `Objects::process_all`, `BuildOut::make_options` and `SelectGroup::add`, each installed when any of the features that use it is on.

## Crash found in testing (2026-10-24)

- Integer divide by zero at 0x006C4BFA in `Leader::create_units` (the AI's unit production): `10 / unittype.pop (+0x2F0)`, also at 0x006C6236. The upgrade units had `POP 0`, and an AI player looked at them.
- A unit's `POP` must never be 0. The upgrades now have `POP 1`, and the mod hooks `LeaderData::check_population(TypeIndex)` 0x006E1330 and `check_population(int)` 0x0046EDA0 (1 = no room). These are called by `Build::do_queue`, `Build::process`, `Build::activate` and the HUD. For upgrade types they return 0, so upgrades never wait at the population limit.
- Windows records game crashes in the Application event log ("Application Error": exception code and fault offset; the offset + 0x400000 is the address in `rise_z.map`).

## More findings (2026-10-24)

- Resource indices (`Type::load_cost` letter table at 0x00663F0C): 0 food (`f`), 1 timber (`t`), 2 wealth (`g`), 3 knowledge (`k`), 4 metal (`m`), 5 oil (`o`). Knowledge's rate cap is always 999 in `calc_resource_caps`.
- `LeaderData::resources_get(r)` 0x0047DA20 reads `[+0x6EB8] + 0x64 + 4*r ^ 0x872`: the income rate (x16), the same field `Leader::gather` stores after `calc_gather`.
- `LeaderData::is_human` 0x006EC170 = `[leader+0] & 4`.
- The AI's building choice (`Leader::create_buildings` 0x006C1BE0) asks `LeaderData::type_avail`, so `type_eligible` returning 0 keeps the AI from a building. The mod uses that to offer the Data Center (type 0x1C1) to a computer player only once its knowledge and wealth income reach `[ai] min_income`.
- Campaign heroes (FLAGS `lmhcbp`, CAT `Command`, OBJ_MASK `MCP1`) were not shown in the Data Center's panel. Plain campaign infantry (`lmji`, `Foot`, `KFR`) is, so every upgrade unit gets those values.
- `buildingrules.xml` OBJ_MASK letter `1` = "can't upgrade to this type" (documented in its header comment, which explains all the mask and flag letters).
- AI names: `Game::init_rules_and_teams` loops over the 8 player slots (`game+0x44`, 0x8C each, flags word at +0x30). It calls `Player::generate_name` 0x006EE3C0 for some of them, which picks a `<LEADER>` from the tribe's XML (`index = (game+0x10 + same-tribe AIs) % count`, or the tribe name if there is none) into `Player+0x40`. Other slots get the localized string at `loc_str_array + 0x6644`. Why AIs sometimes show "Computer" (found 2026-10-24): the naming loop (0x0058A34D) runs only when the first argument is 0, and `Game::run` passes `game+0x544 != 0` (call at 0x00584885). Player flags word (+0x30): bit 1 in the game, bit 4 human; a computer gets `generate_name` (not in Conquest), the first human keeps his name, other humans get the "Player %d" string (loc +0x6644). With game+0x544 set every computer keeps the setup's "Computer". What game+0x544 means is not known yet. ai_names.c also wraps the call in `Game::run` and names every computer still called "Computer" right after it.
- Shown but disabled: `BuildOut::make_options` hides a unit when `type_eligible` returns 0. Otherwise it adds the option, disabled when `BuildData::can_make(type, 1)` (0x0062DB10, through `type_avail`) is below 4. The mod returns 1 for upgrades that are done or not yet reachable, so each branch is a full row like the Library's. `panel.c` draws a padlock on locked ones and a green tick on done ones (`dc_upgrade_state`).
- AI names, more: `init_rules_and_teams` is called with `game+0x544 != 0` as its first argument (likely "loading a saved game"); with it set, the naming loop is skipped. `Player::nameWithPlatformSymbol` 0x006EE1D0 returns `Player+0x40` (prefixed with a platform symbol when `Player+0x54` matches a string). `camera-smooth/src/ai_names.c` calls `generate_name` again the first time a computer player's name is shown in a game, and logs the old name ("ai names:" lines) to find out where "Computer" comes from.
- AI names, the score list (found 2026-10-25): the players already have their leader names right after `init_rules_and_teams`. The "Computer" in the score list is a placeholder on purpose: `StatWin::setup_normal_entries` 0x007ECC40 (called by `StatWin::on_redraw` every frame, through `setup_entries`) writes `loc_str_array_orig + 0x10BE4` instead of `LeaderData::get_name` for a player on another team, not human (flag 4), whom you have not met yet (`dword [leaders + 0x94 + 4*(me*0x1BBB + them)] & 1`), unless the game type byte `[game+0x24]` is 3. The real name shows once you meet them. Kept: the leader's name would give away the nation.
- AI names, the loading screen (found 2026-10-27): `LoadingWin::draw_other_player_info` 0x00834550 hides enemy computers the same way. For a player in the game (leader flag 0x10), not the local one, on another team (`movsx byte [game + 0x78 + me*0x8C]`), not human (flag 4), unless `[game+0x24]` is 3 or the local player's flags word (`game + 0x74 + me*0x8C`, Player+0x30) has bit 8, it writes `loc_str_array (*0x00C8CD00) + 0xC9CC` ("Computer") instead of `LeaderData::get_name`. Two copies: `je` at 0x008348BB (layout with teams) and 0x00834DD8 (the other layout, no team test). The nation is hidden too: at 0x00834A07 the same test picks one of five placeholders (`+0xC9E0`, `+0xC9F4`, `+0xCA08`, `+0xCA1C`, `+0xCA30`, chosen by leader flags 0x20000000 / 0x40000000 / sign bit / `[leader+4]` & 0x400) instead of the tribe name (`*0x00E7FA34 + 0x18 + tribe*0x5F0`). Turning each `je` (0x74) into `jmp` (0xEB) shows the real name / nation. In the score list the name tests are the `je` at 0x007ECF6C and 0x007EDDAD (after the game type test), and the " (nation)" after the name is added only once met (`je` at 0x007EDE15). `reveal_ai_players` (ai_names.c) switches all seven at the start of each single-player game (in the `init_rules_and_teams` wrapper, `game+0x820` bit 4 = multiplayer) and puts them back for multiplayer.
- Queued counts: `Leader::track_queued(type, delta)` 0x006E0F30 keeps `word [leader + 0x5A22 + 2*type]` for every type, units included, so it also says whether an upgrade is already in a queue (`LeaderData::get_queued` 0x006E0D50 adds the grafted types). datacenter.c disables a one-time upgrade while it is queued.
- Fixed 999s outside `calc_resource_caps` (immediate `0x3E70`, rates being stored times 16): `IFaceResources::draw_cap` 0x00819370 starts its minimum over the resources' caps at 999 (`mov ebx` at 0x0081938C), so the cap shown at the top never went above +999; `Leader::do_gather` clamps the rate to 999 on the path of tribe bonus 0x16 (interest on the treasury: the Dutch), `mov eax` at 0x006CE706; `Options::describe` clamps the gather rate it shows at 0x0072A802 and 0x0072A816. datacenter.c (`lift_999`) sets the four immediates to 0x7FFFFFF0. `do_gather` also marks a resource whose cap is at least 999 as "fully capped" (0x006CE5FF, the cap indicator turns red); that one is left.
- Commerce cap: `Leader::do_gather` 0x006CE450 clamps each resource's net rate to `LeaderData::resource_cap_get` (at 0x006CE512, `[secure + 0x30 + 4*r] ^ 0x1281`) and shows the cap message for the local player. A bigger cap changes income only for a resource gathered above it.
- Dutch interest (tribe bonus 0x16, found 2026-10-26), in `Leader::do_gather` 0x006CE62F..0x006CE712, for every resource except knowledge: the rate is first clamped to the cap as usual; then `excess = treasury - starting amount` (`[game + 0x600 + 4*r]`, multiplied in some team modes); if `excess > 0` the rate gets `+ excess * DUTCH_INTEREST / 100` (constants +0x8A8, `rules.xml` 5%: +1 per 20 in the treasury, not steps of 100), clamped to `cap + DUTCH_INTEREST_CAP` (constants +0x8AC, 50), then to 999. Gather handicap and difficulty scaling come after. `Leaders::process_all` runs every `Game::do_frame` and calls `Leader::gather` for every leader, so the treasury gets `rate / GATHER_RATE` (constants +0x27C, 450 frames) every frame, the remainder kept in `[secure + 0x18 + 4*r]`: interest flows in continuously, never as a lump.
- The "+X" rate on the resource bar (`IFaceResources::draw_resource` 0x00818840, at 0x00818BD9) is `([secure + 0x94 + 4*r] ^ 0x90236 + 8) >> 4`, which `do_gather` writes at 0x006CE723 after the interest and the caps (before handicap and difficulty scaling), so it includes the Dutch interest. The yellow "at cap" colour comes from `[secure + 0x4C + 4*r]`, set before the interest is added. `Options::describe` (tooltip) also checks tribe bonus 0x16.
- Building counts per player: `word [leader + 0x5222 + 2*type]` = owned, `word [leader + 0x5A22 + 2*type]` = being built (`ScenarioFuncSet::num_type_with_queued` adds the two; units use +0x5A22 and +0x56FE). The mod blocks a second Data Center with them (`type_eligible` returns 1 when the sum is not 0). `BUILD_FLAGS j` only limits one per city.
- Placement checks: `BuildTypeData::blocked_site(Coord x, Coord y, int player, int, int *)` 0x00636A50 returns 0 when the site is fine, otherwise a message code (0x17, 0x22, 0x24, 0x2D, ...). It is used by `Group::action_build`, `GroupData::validate_build`, the AI (`Leader::produce_building`) and the placement cursor. `blocked_town` 0x00636D80 is a wrapper. This is where a "capital only" rule would go: also needs the city that owns the site and the player's capital (`LeaderData::find_capital` 0x006EB930, `holds_capital` 0x006D54F0).
- `SUPPORT` in `unitrules.xml` (for example heroes' `20m/20g support`) is added to the price the game shows and charges, so the upgrades use plain `support` (none).
- **Tab (found 2026-10-25).** `Options::do_cycle_research(int, int)` 0x0071E7D0 goes through 16 building types: Library, Senate, Temple, Market, University, Granary, Lumber Mill, Smelter, Fort, Tower, Barracks, Stable, Siege Factory, Dock, Missile Silo, Airbase. The list is at `Options +0x30` and the index at `+0x70`, taken modulo 16. For each type it looks for a building of the local player with a technology there (`[tech +0x40]` = the building type, not had, not being researched, `has_preq`, `type_eligible`), or a unit-line upgrade (`[unit +0x40]` = the type and `type_avail(unit, 1) == 2`). It selects the building (`SelectGroup::clear(-1)`, then vtable +0x18 `add(id, player, 0)`) and moves the camera there (`GroupData::get_loc` 0x0070E030, then the Camera fields that `Camera::pan_to(x, y, steps)` 0x00843E00 sets). The Data Center's upgrades are units to train (`type_avail` 4), so the cycle never stops at it. datacenter.c adds that stop when the game's cycle starts over.
- `TypeData::get_cost(resource, player, 5 more)` 0x00664090 (virtual, `ret 0x1C`, only called through the vtable) is the price used for the tooltip, `can_pay_cost` and paying. The mod scales it for repeatable upgrades: `base * (100 + repeat_cost_growth * repeats) / 100`.
