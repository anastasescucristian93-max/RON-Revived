# Smooth Camera

Smooth camera for Rise of Nations: Extended Edition (Steam, `riseofnations.exe` built 2024-06-20, timestamp `6674863F`).

## What it fixes

| Problem in the game | Cause (in `riseofnations.exe`) | Fix |
|---|---|---|
| Diagonal scrolling is √2 ≈ 1.41x faster | `check_scroll` calls `scroll_dir` once per direction and the vectors add up | Directions are collected into one normalized vector |
| Keyboard + screen edge scroll at double speed | The same direction is added twice | Directions are deduplicated |
| Scrolling starts and stops abruptly | Constant velocity, no inertia | Exponential acceleration and braking, frame-rate independent |
| Zoom moves in steps | `zoom_in_times` jumps straight to `level[i]` | Distance is animated (in log space); tilt is interpolated |
| Zooming in jumps to the cursor | `zoom_in(x,y)` pans to the point under the mouse | The point under the cursor stays fixed on screen |
| Buildings vibrate against the terrain while zooming or scrolling | Buildings are 2D sprites over the 3D terrain: `world_to_console` rounds their screen position to whole pixels, `SpriteMethod::execute` truncates the hotspot offset, `Sprite::draw` takes ints | `subpixel_sprites`: the same projection is done in float around `draw_build` and the dropped fraction is added to the sprite's quad in d3dgl, right before `clip_verts_ordered` |
| Camera gets stuck moving in one direction | `GetKeyboardState` misses the key-up when focus is lost | Scroll keys are checked against `GetAsyncKeyState` |
| Jagged fog of war | Fog is a half-tile grid (values 0..3); each vertex gets the 2x2 average quantized to 13 levels, and `fow_fade.tga` drops sharply at the edge of vision | `fill_fog_texcoords` gets a Gaussian-weighted, unquantized value per vertex, warped so the game's own ramp fades smoothly; `invalidate_fog_cache` also refreshes the 8 neighbouring tiles; the ramps (`fow_fade.tga`, `fow_fade_dark.tga`) are rewritten straight above "explored" into `%TEMP%\RON Revived\` and served through the file overrides, so the pixels between vertices do not zigzag |
| No overview of where a building fits | Placement mode only colours the footprint under the cursor | Build grid: every tile of your territory within `grid_radius` of the cursor is drawn green (free for the selected building) or red (blocked), with gaps between tiles as grid lines |
| Flat, soft-looking picture; anti-aliasing that does nothing with bloom off | The tone mapper's output goes straight to the screen; FXAA reads luminance from alpha, which only the bloom pass writes | `postfx`: colour (black level, contrast, vibrance, warmth, vignette) added to `ps_tonemap.hlsl` and contrast-adaptive sharpening to `ps_fxaa.hlsl`, both generated from the game's files into `%TEMP%\RON Revived\shaders\` and served through the file overrides; the tone mapper writes luminance into alpha. Interface untouched. Ctrl+F9 off / on, Ctrl+F10 half-screen comparison; `tools/postfx_preview.py` shows the same math on a screenshot. Water (`ps_global.hlsl`, only the `ANIMATE_WATER_NORMALS` permutations): screen reflections that reach about 9 tiles instead of half a tile, sun glints, sky tint, livelier waves |
| A loaded game starts at once, before you can read the loading screen | `Game::run` calls `CommandManager::start` (which closes the loading window) as soon as loading ends | `play_button`: that call first waits, drawing the screen with `Window::modal_idle_func` as the loading does, and the progress bar becomes a PLAY button (click, Enter or Space). Single player only |

| Tiny interface on high-resolution screens | The 2D interface is laid out in physical pixels | The renderer's virtual-resolution scale (`graphsys` +0x350..+0x364), which the game only ever set below 1 for screens under 1024x720, is set to the Windows display scale (or `ui_scale`), so the interface and map are laid out at physical / scale and drawn at native resolution |

Interface scaling sets the scale before `GraphSys::init_display_quick` and re-applies it in d3dgl's `D3D11Context::change_resolution` (mid-function hook right after the game resets it). Two latent bugs of the scale > 1 path are patched: `Window::recurse_clips` multiplied the already-logical screen rect by 1/scale again, and the main menu (`TopMenu`, `TopMenuSelection`) positioned its logo and boxes from the physical size. In exclusive fullscreen below the desktop resolution the auto scale is reduced by the monitor's own stretch.

The build grid hooks `BuildTypeOut::draw_template` (the footprint drawn every frame of placement mode) and draws first, so the game's footprint stays on top. Free/blocked is `BuildTypeData::blocked_tcoord`, the per-tile check `blocked_site` runs on every footprint tile, so it follows the game's rules for the selected building (docks, farms, etc.). Territory is the owner byte of the world cells (4x4 tiles each), as in `non_friendly_territory`. Tiles are the terrain's own two triangles per tile, inset for the grid lines, sent through the graphic card with the footprint's render state.

The game's `adjust_tilt` has a bug (it uses `dist - tilt[i]` instead of `dist - level[i]`) that never showed while zoom was stepped. The mod computes the tilt itself.

## How it works

`winmm.dll` is a proxy: all 193 exports forward to the system `winmm.dll`. On load it checks the exe timestamp and the bytes at every hooked address; if anything doesn't match, no hook is installed. Hooks are in-memory `jmp`s, so the exe on disk is never modified. Addresses come from `sbl/rise_z.map` (shipped with the game) and are adjusted for ASLR.

| Function | Hook |
|---|---|
| `Camera::scroll_dir` | replaced: only records the direction |
| `Camera::check_scroll` | wrapper: calls the original, then applies the movement |
| `Camera::check_zoom` | replaced for the main camera (Conquest keeps the original) |
| `Camera::zoom_in(x,y,z)` | wrapper: turns the pan-to-cursor into an anchor; at level 6 takes the extra close step |
| `Camera::zoom_in()`, `Camera::zoom_out(mask)` | wrappers: enter and leave the extra close step (`extra_zoom`) |
| `BuildTypeOut::draw_template` | pre-hook: draws the build grid, then runs the original |
| `GuyData::get_render_angle` | replaced (`facing.c`): the game's interpolation, plus one between ticks for aircraft, whose angle is snapped every tick |
| `GuyOut::draw` 0x005DC880, `draw_shadows` 0x005DC422 | patched (`facing.c`): the model's and the shadow's facing from the exact angle instead of 256 whole-degree steps |
| `Console::execute_at_cursor`, the Spy's `GroupData::count` call 0x007C947A | redirected (`bribe.c`): with no Spy selected, a Senator (Bribe's FROM2) counts as one when the enemy unit under the cursor can be bribed, so it gets the bribe cursor and the right-click Bribe |
| `Unit::work`, its `Unit::check_target_path` call 0x0060D707 | redirected (`follow.c`): a unit walking to cast on a unit stops the walk once in range (it casts) or when the walk ends out of the target's range (`cast_follow`); a follower drops a walk that ends more than 2 tiles beyond its distance from the target (`escort_follow`) |
| `Unit::work`, the guard branch 0x0060D694 (the 64-frame repath) | replaced (`follow.c`, `escort_follow`): calls `c_guard_tick`, which keeps the 64-frame repath and, every 16 frames, drops an escort's walk that ends more than 4 tiles beyond its place by the guarded unit |
| `LeaderData::has_tribe_bonus`, `Tribes`, `Game::init_tribes`, the setup screen's nation lists | patched (`romania.c`, only with `romania.ini` and a 25-nation rules.xml): Romania as tribe 24, the Random choices moved to 25-29, Romania's power pieces (`romania/ENGINE_NOTES.md`) |
| `Unit::resolve_unit_collision` | hooked (`shove.c`, `make_way`): a moving unit blocked by an idle unit of its own player moves that unit's squad aside with `add_move_order` to a free spot, then the game waits for it |
| `Unit::detect_unit_collision`, at its move group test 0x00617676 | hooked (`shove.c`, `pass_through`): a land unit of the mover's own player or of an ally (`pass_allies`) is walked through (the game's own pass-through branch) when it moves, or when it stands and the mover is more than `pass_clear` tiles from the end of its path |
| `Group::action_move_near`, the formation pick 0x007050FA and the units' formation store 0x0070577A | patched (`formation.c`): a human player's group of more than one unit type (Generals and Supply Wagons not counted) gets formation 9, the game's "Mob": separate move orders, own speeds, ring placement; the units' remembered formation is left alone |
| `Form::compute_rows_and_columns` | wrapper (`formation.c`, `compact`): after the game's sizing, the front of the row formations (kinds 0-5) is narrowed to a block `width_ratio` times as wide as deep |
| `TypeData::get_cost`, the count of buildings owned 0x00665966 | replaced (`defenseramp.c`, `defense_ramp_whole_line`): for a type of the Tower line (0x1B7-0x1BA) or the Fort line (0x1BB-0x1BE) the price increase counts the four types together |

## Build

```powershell
python -m pip install ziglang
.\build.ps1                    # build\winmm.dll
```

The players' package (the DLL together with the data mods and the art) is made by `package/build_package.py` at the root of the repo.

`tools/gen_winmm_proxy.py` regenerates `src/winmm_proxy.inc` and `src/winmm.def` (needs `pefile`).
