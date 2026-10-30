# Engine notes: entrenchment

Findings of 2026-10-25, from `riseofnations.exe` (Steam EE, timestamp 6674863F) with the names of `sbl/rise_z.map` (`tools/rdis.py`).

## The game's own entrench (the General's ability)

- A unit is entrenched when `[unit + 0x68] & 0x2000000`. `UnitData::is_entrenched(int strict)` 0x0060A810: with `strict` it also needs `[unit + 0x6C] & 0x20000` to be clear (that bit marks the "soft" entrench of idle units, see below).
- `SpellType::cast_entrench(int, int)` 0x00672820 (the General's craft) entrenches every idle unit of the player in the General's radius (`HeroData::get_radius`) whose type can (`UnitTypeData::can_entrench` 0x0061D470: objmask 0x20, a weapon `[type + 0x1E8]`, `[type + 0x2C8] & 0x10000`). For each soldier of the squad (see below) it does: `Unit::close_orders(0)` 0x005E37F0, `Unit::clear_partial_path()` 0x005E3920, `Unit::update_action()` 0x0060A870, `[unit + 0x68] &= ~0x4000000`, `[unit + 0xC0] = 0`, `GraphicEvents::add_entrench(player, id)` 0x008E4670 (the dirt mound, only if not already entrenched), `[unit + 0x68] |= 0x2000000`, for Antipater `[unit + 0x6C] |= 0x1000`, and `[unit + 0x5C] = [unit + 0x50]` (the facing it dug in with).
- Idle units with `[unit + 0x6C] & 0x800` dig in by themselves after `Constants + 0x874` idle ticks (`Unit::check_idle` 0x006032C0): the American Marines (`AMERICANS_MARINE_ENTRENCH` in `rules.xml`). They also get `0x6C |= 0x20000`.
- **Effects.**
  - Damage taken: `ObjectData::get_damage` 0x00644130 (this = attacker; target id, target player, attack angle, flag, ?, out) multiplies by `Constants + 0x64` / 256 = `ENTRENCHMENT_MODIFIER` (2/3) when the target is entrenched, unless the attacker `is(0x83, 0)` (Flamethrowers), and only when the attack comes from the front: the angle between `[target + 0x5C]` and the attack angle must be within about 60 degrees, or the fourth argument is set. Antipater's units (`0x6C & 0x1000`) get `Constants + 0xB98` on top.
  - It stays put: `Unit::fight` 0x005FD4D0 (at 0x005FE157) does not look for an attack position while the unit is hard-entrenched, so it never walks after targets; `Unit::find_melee_target` (0x005FFAB6) has the same test.
  - It ends: `Unit::move_step` clears it when the unit moves; `Object::do_damage` 0x0064A480 (at 0x0064BC6E) clears it and calls `GraphicEvents::remove_entrench` 0x008E45D0 when a Flamethrower (`is(0x83, 0)`) hits; `Unit::close` 0x0060EE60 on death.
  - The flag is part of the unit's state, so it is in saved games.
- `GraphicEvents::remove_entrench(player, id)` searches its list and removes the entry if there is one, so it is safe to call for a unit that never got a mound.

## Squads

- A squad ("uber unit") is a chain of separate unit objects, one per soldier. `word [unit + 0x8E]`: bit 15 set = the captain; otherwise the captain's id. `short [unit + 0x90]` = the next soldier, -1 at the end. `UnitData::curr_uber_size` 0x0060A760 counts them; `UnitData::is_captain` is virtual +0xE8.

## Garrison

- `Unit::do_garrison(UnitOrder *)` 0x005E6B80 runs a garrison order. The order's data (virtual +0xAC on the order) has the target id at +8 and its player at +0xC. Each frame it checks the building (allied, garrison limit `BuildTypeData::get_garrison_limit`, `UnitTypeData::can_garrison(building type)`), then `Object::adjacent_to(id, player)` (virtual +0x170, 0x00651F40): when adjacent the unit goes in (0x005E7025), otherwise it walks on.
- `UnitData::can_garrison(building id, player)` 0x006099A0 (ret 8) is what the right-click cursor, `GroupData::test_garrison` and `GroupData::count` ask.
- The eject button is option 0x1F at (3, 0), added by `BuildOut::make_options` when the building has units inside (`ObjectData::num_inside(1)`). Clicking it calls `Options::do_eject_all(int)` 0x0071C470, which checks `GroupData::count(0x11, 0x140 / 0x13E)` and then sends `GroupOut::issue_eject_all(0, int)` 0x00708B90; on every machine `CommandPackage::process_eject_all` calls `Group::action_eject_all(int, int, int, int)` 0x00710B40 (ret 0x10).

## Untargeted crafts as buttons

- `Options::do_spell(type, int)` 0x0071D7A0: after `Group::can_activate(1, -1)`, `GroupData::get_spellcaster` and `get_valid_spellcaster`, a craft with none of the target flags (`[craft + 0x1C8] & 0xE` = 0: no b / c / d in craftrules.xml) goes straight to `Options::target_spell(type, -1, -1, 0, 0)`, so it is sent as the game's spell command at once, with no cursor; `Group::action_spell` then runs on every machine. Unused crafts of that kind: Restock Supplies 0x27E (icon cell (7, 4)), Research Project 0x285 (its cell (0, 6) is Raze's too), Move Capital Here 0x287 (1, 6). With `PREQ0 None`, `FROM None` and an `Options::add(4, column, row, craft, 0, 0)` from a panel hook, the button shows only where the DLL puts it.
- **A cell of `iface_unit_orders.tga` (TEX_ORDERS_0) is not free just because no craft names it:** the game's own buttons draw cells from code. Restock Supplies' cell (7, 4) is the cities' rally-flag button (seen in game 2026-10-25, after the Entrench icon was painted there). A craft's `TEX_ID` may name any icon page (`TEX_ORDERS_0` is written out for some crafts), so the buttons' icons went to empty cells of `TEX_UNITS_2` (`iface_units_03.tga`, (6, 10) and (7, 10)), which no unit uses.
- `UnitOut::make_options()` 0x00615FC0 (virtual) builds a unit's panel.
- `Unit::add_garrison_order(building, player, int, QueuePos, int)` 0x005E4080: `Group::action_garrison` calls it per unit with (building, player, 0, queue, 1); scripts give the group queue 2.

## Combat values

- `UnitData::max_range()` 0x0060FE90 (virtual): the type's `[type + 0x1FC]` plus tribe, wonder and General bonuses, in the same units as `RANGE` in `unitrules.xml` (13 for Infantry). Melee units have 0.
- `UnitData::min_range()` 0x006100B0: above 0 for artillery, aircraft and bombard ships only.
- `UnitData::recharge(int *)` 0x0060FDF0 (virtual): the type's `[type + 0x1F4]` in ticks, 1.5 or 2 times that out of supply.

## Moving a unit

- `Unit::set_new_location(Coord x, Coord y, 1, 1)` 0x005F8D20 (ret 0x10) puts a unit somewhere at once: it takes it out of the world, sets the position and the model's height from the terrain, and puts it back. The debug "jump to action" uses it with the target building's own position, so a unit can be put on a building's footprint.

## Art

- A building's depth comes only from its `zbuffer` mesh (see `roman-buildings/ENGINE_NOTES.md`). Depth meshes are in world units, z up, the building's centre at the origin; a 2x2 building's mesh spans about -42 .. 42 in x and y.
- Projection fitted on the game's towers and lookouts (mesh against sprite, origin at `spritexoff`, `spriteyoff`): sx = x0 + 0.76 (x - y), sy = y0 + 0.30 (x + y) - 0.90 z, in sprite pixels. The fit is rough (the towers' bases do not fill their footprints); the Nest's mesh will be tuned in game. Refit on 11 buildings (2026-10-25, `roman-buildings/ENGINE_NOTES.md`, "Projection"): sy = y0 - 0.31 (x + y) - 0.90 z, sx with 0.70; larger x + y is the back. The Nest's ring is symmetric, so its mesh is not affected.
- The Infantry model (`inf.bh3`) is about 16 units tall.
- No building in `building_graphics.xml` has an `AGE7` entry: the Modern and Information Ages share `AGE6`. The Information Age look is swapped in at draw time by `nation_art.c` (`[all]` section of `nation_art.ini`, `nest_6.tga@8`), so the swapped sprite must keep `nest_6.tga`'s canvas and origin (`make_nest.py --like nest_6`) and lives with its depth mesh.
- Building icons: `TEX_BUILDINGS_3` = `art\iface_buildings_wonders.tga` (256 x 256, cells 61 x 39 on a 62 x 40 step; `_half` is half that). The Lenin Statue's cell is (1, 5).

## Who may garrison where (first test, 2026-10-25)

- In the first test the Entrench button sent the squads (log "sent to Nest") but they never arrived: `Unit::do_garrison` asks `UnitTypeData::can_garrison(building type)` 0x0061D7D0 every frame and drops the order when it says no. For land units (`[type + 0x218]`, the DOMAIN, is 0) it says yes only for the City line (`is(0x19E)`), the Fort line (`is(0x1BB)`) and the unit's own production building (`[type + 0x40]`, with patch-version cases for Barracks, Stable, Auto Plant, Siege Factory and Factory); ships (DOMAIN 1) only for the Dock line (`is(0x1B0)`). The Lenin Statue is none of these. `UnitData::can_garrison` (right-click cursor, `GroupData::test_garrison`) calls the same function, so right-click did nothing either. Fixed by hooking it: for the Nest, ranged foot soldiers.
- `is(type, 0)` on a type with another type index tests the line: building indices start at 0x19E (Small City) in `buildingrules.xml` order (Farm 0x1A1, Barracks 0x1AB, Dock 0x1B0, Tower 0x1B7, Fort 0x1BB, Airbase 0x1BF, Lenin Statue 0x1C2).
- OBJ_MASK letters are bits from A: bit = 1 << (letter - 'A'), stored at `[type + 0x1E4]`. F (0x20) = foot soldiers (`UnitTypeData::can_entrench` tests it); tanks and guns have no F.

## The garrison targeting (the Entrench button)

- The game has a garrison targeting mode: option code 0x1A (a garrison button some units get at (1, 3)) makes `Options::exec` call `Options::do_garrison(int)` 0x0071D6B0, which sets cursor 7 (the garrison cursor) and `Options + 0x24` = 1. `Options::exec` first copies the clicked option to `Options + 0x7C` (code, then type at + 0x80), and `Options::picked_spot` 0x00721C40 acts on that code: for garrison, `Console::find_target(&player, 4, 0, 2, ...)` (search 4, filter 2 = buildings), `get_garrison_limit`, `GroupData::test_garrison`, then the garrison command.
- So a craft button can start it: hook `Options::do_spell(type, int)` 0x0071D7A0 (ret 8), write 0x1A / -1 to `Options + 0x7C` / `+ 0x80` and call `do_garrison(0)`.

## Building (the Dig button)

- Craft FLAGS letters are bits from a (`[craft + 0x1C8]`): b 2 = targets units, c 4 = targets buildings, d 8 = targets the ground. `SpellTypeData::find_target` 0x00674920 also reads e (0x10), f (0x20), h (0x80) for the search and i (0x100), j (0x200) for the filter. With d, `picked_spot` calls `target_spell(type, -1, -1, x, y)`.
- The build menu: `UnitOut::make_options` with `Options + 0x74` = 1 lists every building type with option code = the category (`Options + 0x78`) at its GRID_X / GRID_Y; a type whose `[type + 0x30]` (PREQ0) is -2 ("disable") is left out. A click on such an option (codes 0..2, `Options::exec` at 0x0071AE13) needs `GroupData::count(1, 0, 0)` (Citizens in the selection), so other units cannot place buildings through it.
- `Group::action_build(x0, y0, x1, y1, type, queue)` 0x00707510: `GroupData::validate_build` (no worker test), `BuildTypeData::snap_center` 0x00636190, `BuildTypeData::blocked_site(x, y, player, -1, 0)` 0x00636A50 (0 = free), the paying city from `ObjectsData::find_city_at(x, y, player, -1, 0)` 0x0065B870 unless `[type + 0x2C0] & 0x2010`, BuildType virtual +0x84 (player, city, -1, 1) = can pay, +0xD4 (player, -1, city, 1, 0) = pay, `Objects::init_build(player, type, x, y, 0, -1)` 0x0065D190 = the site's id (it snaps the centre itself), then `Group::action_swarm_around(site, player, queue, 6, 1)`. That gives build orders (6, 0xD) only to Citizens (types 0x32, 0x33) and units that can cast craft 0x293, so soldiers get none.
- `Unit::do_build` 0x005EEBF0 works on a site each frame with `building->virtual +0xB0` (the Wall part) `->do_construct(amount)` 0x006434D0, amount `Constants + 0x22C` (less in some cases: a site flag, a tribe bonus); it returns non-zero when the building is finished. A site that has not started checks `blocked_site` first, so builders must stand off the footprint. The DLL does the same for idle ranged foot soldiers near a Nest site.
- Type indices: techs are 0x220 .. 0x274 in `techrules.xml` order (Modern Age 0x225, The Art of War 0x23C), crafts from 0x275.

## Second test (2026-10-26) and the game's own build button

- The Dig craft (ground target) showed the generic target cursor, no outline and no price. Replaced by the game's own build button on the infantry panel: `Options::add(1, column, row, 0x1C2, disabled, 0)` (codes 0..2 are build buttons; the building's icon, price tooltip, building cursor and ghost come with it). `Options::exec` lets a build button through only when `GroupData::count(1, 0, 0)` 0x00711720 (Citizens in the selection) is non-zero, called from 0x0071AE76 (return address 0x0071AE7B, the clicked option's type in `Options + 0x80`); after that the path to the placement is the same for any unit. `Options::picked_spot` then checks `validate_build` and sends `GroupOut::issue_build`, without asking for Citizens.
- For that the type must be available (`LeaderData::type_avail` = 4: `[type + 0x30]` and `get_preq` above -2 in `Options::exec`, `type_avail` again in `Group::action_build`), so PREQ0 is The Art of War again. Kept away from Citizens by leaving it out in `Options::add` 0x0072C290 (ret 0x18), and from computer players by answering 0 in `LeaderData::type_avail` 0x006E33A0 (ret 8) when called from `Leader::create_buildings` 0x006C1BE0 .. 0x006C40A0, which picks the buildings they plan. They build through `Leader::make_this` -> `Leader::produce_building(type, city, int)` 0x006E1400 -> `Objects::init_build`.
- `Object::do_damage(target id, target player, angle, int, ammo, int, int, int)` 0x0064A480 (ret 0x20): `this` is the attacker; it calls `this->get_damage` against the target. Changing the target arguments at the entry makes the hit land on another object with the attacker's damage against it.
- Max hit points: `Object::update_hits(int)` 0x00647010 (ret 4) stores the type's HITS (`[ObjectType + 0x210]`) in `[object + 0x20]` and returns it; `Wall::update_hits` 0x0063F0D0 adds the building bonuses (tribe, `get_building_hp_upgrade`, wonders) and stores the result in `[object + 0x54]`, which `BuildData::hits(0)` 0x0062E740 returns. `Wall::update_hits` runs at `Wall::init` and in `Wall::inc_time`, so a changed base reaches existing buildings.
- Splash: `Ammo::do_damage()` 0x00678060 (`ebx` = the Ammo) calls `Object::do_damage` for every object in the burst; its seventh argument is 0 for the shot's own target and 1 for the others (a hit without splash passes 0x100, 0, 0). `Object::do_damage` itself spreads an eighth of a hit around with the eighth argument 1 (0x0064C4B7). So the Nest counts as one target: splash on its soldiers is dropped, and one shot (keyed by the Ammo, from the hook's saved `ebx`, or by the attacker for hits without an Ammo) hits the Nest once per frame (user, 2026-10-26).
- To check in game: whether the ghost and the build cursor appear for infantry.

## Third test (2026-10-26)

- Building worked; the squads by a finished Nest went in (only those counted before the last bit of work in the finishing frame: now all of them). Several squads were put on place 0 of the same Nest: soldiers get pushed a little, so `free_squad` found no captain within `soldier_spread` of a place. It now counts the captains in the table for that Nest and gives each the nearest place.
- User's rules: the Entrench button is gone (only the build button stays); getting in is the game's garrison command, right-click or its garrison button (option 0x1A at (1, 3), `Options::do_garrison`, its cursor and hotkey), which the infantry panel gets once the player has a Nest unless the game added it already (seen in the `Options::add` hook).
- "All Military" (`Options::do_cycle_military` 0x0071FDE0, the qol.c Army button) did not select dug-in soldiers. It takes a unit alone only when its move group, `short [unit + 0x80]` (`Unit::set_group(group, 0)` 0x00605220 sets it for the whole squad), is -1, and units in a group through the groups list. Dug-in squads keep the group of the move that brought them, so they are taken out of it (`set_group(-1, 0)`, as the scenario command `unit_clear_orders` does) when they dig in, and in the frame update for squads dug in before. To confirm in game.

## Fourth test (2026-10-26): soldiers on the edge of the Nest

- Screenshot of an age 5 Nest (`nest_4`): the squad the log put on place 0 (`place_radius_age5` = 55) stood about a tile from the centre, on the footprint's edge at the front-left wall, while its place is well inside. Fitting the sprite on the screenshot (scale 1.52) put every place of the code inside the ring, around the origin, so the art's origin was not the cause.
- `Unit::set_new_location` 0x005F8D20 puts the unit exactly where asked: no search for free ground (`remove_from_world`, position, `find_tcoord_z`, `add_to_world`), and it moves the models too (`Guy::set_new_location` 0x005D86F0; one model: `guy + 0x5C / 0x60` = the unit's x / y, plain; several (`byte [unit + 0xB5]`): a formation around it). So something moves the soldiers later. Candidates, not checked: `Unit::resolve_unit_collision` 0x005F9D30 (it calls `UnitData::invalid_loc`, which a solid footprint fails), the end of the garrison order that `c_en_do_garrison` cuts short.
- The DLL now keeps every soldier's place (`g_spot`) and `refresh` puts a dug-in soldier back when it is elsewhere (`hold_place`, keeping the entrench flag). The first move of each soldier is logged with how far the unit and its model were from the place ("moved off its place by"), which tells what moves it.
- In `nest_4` the origin lies on the front walls' line; the courtyard drawn above it is mostly behind the footprint's centre, as the walls are high.

## Fifth test (2026-10-27): one squad in a line on one wall

- Screenshot: the one squad in a Nest stood in a line along one wall. The log had no "moved off its place" for it, so it stood where the code put it (place 0 was a line along the +y wall). The user wants the soldiers over the whole floor.
- User's rules (2026-10-27): one squad per Nest (`SQUADS` 1, `GARRISON_MAX` 3 in buildingrules.xml, so the game's own garrison command refuses a second squad as for any full building), and its three soldiers evenly spread. They stand on an equilateral triangle `place_radius` from the centre (100; 55 in age 5), map directions (-0.707, -0.707) for the captain (back corner), (0.966, -0.259) and (-0.259, 0.966) for the other two (front right and front left on screen). `soldier_spread` is gone. `tools/check_mesh.py` draws them: at 100 they are on the floor of every look, clear of the walls. (A 3 x 3 grid for 3 squads was tried first, the same evening, before this rule.)
- The same log: "Nest 2035 dug, 2 squads go in", then "squad 61 refused at Nest 2035 (full)" and "squad 61 into Nest 2035" in the same second, and no line for the second squad. `work_on_site` sends the squads (`add_garrison`) in the frame the Nest is finished; they reach `c_en_do_garrison` before the next `refresh`, while the table still lists the Nest as a site, so `free_squad` found no index and answered "full". It now appends a finished Nest missing from the table (`add_nest`); a full rebuild there would shift the other Nests' indices in `g_in` for the rest of the frame. Why the second squad never logged anything is not known; to check in the next test.
