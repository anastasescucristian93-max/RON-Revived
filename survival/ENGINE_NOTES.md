# Survival: engine notes

What the survival script relies on, found 2026-10-29 in the game's own scripts (369 `.bhs` files under `scenario\` and `conquest\`) and the `ScenarioFuncSet::` symbols (879, in `tools/rdis_syms.txt`).

## Game scripts in the setup screen

- Every folder under `scenario\Scripts\` with a `.bhs` in it is a choice under **Script** in the custom game setup (the game ships `Auto_Pause` and `No_Nukes`; our `Art_Test` works the same way). The folder name is the name shown.
- The game style list (Survival of the Fittest = `GAMESTYLE1` in `Data/help.xml`, the vote button in `interface.xml`) is fixed in the exe; a new style would need the DLL.

## The script language

- C-like. A `scenario { ... }` block; its plain code runs on every script pass, `run_once { }` once, `trigger name (condition) { }` once until `enable_trigger("name")`. `static` variables keep their value between passes; arrays: `static String a[] = ["x", "y"];`, `a[i]`, `a.length`. Strings join with `+` (also with ints). `rand_int(lo, hi)` inclusive. Library files are `include`d (`scenario\scriptlibrary\`).
- Timers: `set_timer("name", seconds)`, `timer_expired("name")`, `stop_timer("name")`.
- Ages: `age(p)` / `get_current_age(p)`, 0 = Ancient .. 7 = Information (`airliftsetup.bhs`: `if (age(1) == 0)`). `gain_next_age(p)`, `lose_current_age(p)`.
- Players: `num_players()`, `num_human_players()`, `is_human(p)`, `is_defeated(p)`, `have_war(a, b)`, `get_leader_difficulty(p)` (the setup difficulty; numbering to be confirmed), `get_difficulty()` (campaigns, 1..5).
- Units: `create_unit_in_group(p, x, y, "type", n)` adds n units to p's default group; `clear_group(p)` empties it; `group_attack_to_order(p, x, y)` attack-moves the group. `create_unit_upgrade` makes the player's current upgrade of a line instead (no use for a player without upgrades). Type names compare without case.
- Iterating: `find_all_military(p)`, `find_idle_military(p)`, `find_all_unit(p, "")` return the next unit on each call; the campaigns loop `num_military_units(p)` times. `find_unit(p, "")` = any unit.
- Cities: `find_capital(p)` returns the capital's **name** (String); `find_city_id(name)` its object id; `object_position_x / _y(p, id)`.
- Map: `get_map_size()` (square), `map_is_land`, `map_is_passable`, `territory_owner(x, y)`, `is_on_screen(x, y)` (the local camera: differs between machines, so only in single player).
- Keeping a player without cities alive: `disable_city_defeat(p)` (the Alexander campaign does it for the attacker). AI parts off: `disable_production_ai`, `disable_city_ai`, `disable_combat_ai`; `disable_take_attrition(p)`.
- Messages: `print_game_msg(text)` (chat area), `show_notice(text, "city_under_attack_notice", 1)` (the announcement with an icon; other icons: `launch_notice`, `capital_notice`, `reinforcement_notice`, `defeat_notice`, ...).
- End: `victory(p)`, `defeat(p)`.

## Compiling (learned the hard way, 2026-10-29)

- A script that does not compile only gives "did not compile successfully or could not be found" in the setup screen. The compiler's messages go to `OutputDebugStringW` (`Compiler::comp_error` 0x009BED80), e.g. `ERROR: survival.bhs (Line 82): Can't convert "bool" to "single". Explicit cast required`. Any DBWIN listener (DebugView, or a few lines of Python on the `DBWIN_BUFFER` / `DBWIN_BUFFER_READY` / `DBWIN_DATA_READY` objects) shows them; the game's own logs do not.
- Line numbers count CRLF line ends only: with LF-only files every error is "Line 1". Keep `.bhs` files CRLF.
- Typing is strict: a comparison or `&&` gives `bool`, which does not go into an `int` (`ok = a && b;` fails). Use `if (...) ok = 1;`. Function results typed `48` in `Data/scriptfunctions.xml` (`timer_expired`, `map_is_land`, `object_position_x`, ...) go into ints and into conditions fine. `float` variables with `(float)` / `(int)` casts work; `sin` / `cos` take degrees.
- `Data/scriptfunctions.xml` lists only the editor's functions (no `set_timer`, `print_game_msg`, `rand_int`...); the compiler knows all 879 `ScenarioFuncSet` ones.
- `time()` is the game time in **minutes** (`[game + 0x560] / 60`); timers are in seconds: `set_timer` stores `[game + 0x560] + seconds` with `ScriptTimers::add_timer(String &, int)` 0x00A049E0, at most 100 timers.
- `is_seen(who, whose, object)` is about an object, not a tile; there is no tile-visibility function. "Out of sight" is approximated with `any_object_near(defender, 1, x, y, 16)` being false.
- `add_flag(x, y)` adds a scenario marker (`ScenarioData::add_component`); it does nothing in multiplayer (`game + 0x820 & 4`).

## The DLL side (camera-smooth/src/survival.c)

- The script tells the DLL what it needs through timer names, caught in `ScriptTimers::add_timer`: `surv_end`, `surv_wave`, `surv_boss` (the clocks) and `surv_zone x0 y0 x1 y1` (the no-build zone in tiles). Saved games restore the timers without `add_timer`, so the script sets `surv_end` and the zone again every minute.
- The clocks are drawn after `IFaceMainBase::draw_timers` 0x0080B060 (every frame from `setup_progress_buttons`; it draws the game's own clock lines right-aligned at screen width - 10, y 45, with font "timer_size", int string 0x1511C) through `Buffer::write_right_l(String const &, x right, y, len)` 0x0050F9C0 on `Window::screen` 0x00ED8010. The screen Buffer's text colour is at 0x00ED80A0 and its shadow at 0x00ED80DC (12-byte Colors), current font at 0x00ED808C.
- The zone: `BuildTypeData::blocked_site(x, y, player, int, int *)` 0x00636A50 returns 0x18 inside it. Every caller (placement cursor, `Group::action_build` on every machine, the computer players, walls) respects it. Its codes and their messages (`BuildTypeOut::say_blocked` 0x00635C50, text from `translated_strings.xml`): 0x01 existing building, 0x08 invalid location, 0x12 must be adjacent to friendly territory, 0x17 cities in friendly territory, **0x18 "Can't build in enemy territory."**, 0x1A within your own territory, 0x22 map boundaries, 0x24 explored territory, 0x2D no new city for 5 seconds. The rest: `say_blocked`'s jump table at 0x006360D4 (index code - 1), each case adding a `translated_strings.xml` offset (entry = offset / 0x14) to `*0x00C8CD00`.
