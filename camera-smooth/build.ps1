# Compileaza build/winmm.dll (32-bit) cu zig cc.
# Necesita: python -m pip install ziglang
param([string]$Python = "python")

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
New-Item -ItemType Directory -Force "$root\build" | Out-Null

& $Python -m ziglang cc -target x86-windows-gnu -shared -O2 -s -msse2 -mstackrealign `
    -Wall -Wno-unused-function `
    -o "$root\build\winmm.dll" "$root\src\ron_camera.c" "$root\src\fog.c" "$root\src\qol.c" "$root\src\nation_art.c" "$root\src\build_grid.c" "$root\src\ui_scale.c" "$root\src\subpixel.c" "$root\src\kills.c" "$root\src\ranks.c" "$root\src\rankfx.c" "$root\src\heal.c" "$root\src\quicksave.c" "$root\src\bigsel.c" "$root\src\placement.c" "$root\src\shove.c" "$root\src\formation.c" "$root\src\hud.c" "$root\src\respanel.c" "$root\src\flags.c" "$root\src\nation_grid.c" "$root\src\panel.c" "$root\src\override.c" "$root\src\crash.c" "$root\src\launcher.c" "$root\src\launcher_lang.c" "$root\src\launcher_update.c" "$root\src\launcher_extras.c" "$root\src\datacenter.c" "$root\src\airdrop.c" "$root\src\airstrike.c" "$root\src\cityrepair.c" "$root\src\entrench.c" "$root\src\ai_names.c" "$root\src\armageddon.c" "$root\src\nonukes.c" "$root\src\unitbans.c" "$root\src\corpses.c" "$root\src\animfix.c" "$root\src\nukefx.c" "$root\src\attrition.c" "$root\src\defenseramp.c" "$root\src\notices.c" "$root\src\minimap.c" "$root\src\selpanel.c" "$root\src\survival.c" "$root\src\cards.c" "$root\src\menuhelp.c" "$root\src\assistant.c" "$root\src\assist_orders.c" "$root\src\assist_panel.c" "$root\src\hover.c" "$root\src\squadhp.c" "$root\src\outline.c" "$root\src\postfx.c" "$root\src\bribe.c" "$root\src\follow.c" "$root\src\rally.c" "$root\src\costs.c" "$root\src\prodbar.c" "$root\src\facing.c" "$root\src\play_button.c" "$root\src\romania.c" "$root\src\lang.c" "$root\src\winmm.def" `
    -luser32 -lgdi32 -lkernel32 -ladvapi32 -lbcrypt -lwinhttp -lshell32
if ($LASTEXITCODE -ne 0) { throw "build esuat" }

Remove-Item -ErrorAction SilentlyContinue "$root\build\winmm.lib", "$root\build\winmm.pdb"
Write-Host "OK: $root\build\winmm.dll"
