# Installs RON Revived into the Rise of Nations: Extended Edition folder (Steam).
# Copies everything in game\ into the game folder. Game files it replaces are kept in
# RON_Revived_backup\ and put back by uninstall.ps1; RON_Revived_files.txt lists what it installed.
# The game folder keeps a copy as RON_Revived_install.ps1 for the launcher's updater
# (RON_Revived_update.ps1), which passes -Payload, its downloaded files, and -Parts, the parts
# of the pack they are: the files of the other parts stay as they are.
param([string]$GameDir, [string]$Payload, [string]$Parts)

$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
if (-not $payload) { $payload = Join-Path $here "game" }
$only = if ($Parts) { @($Parts -split "," | ForEach-Object { $_.Trim().ToLower() }) + "meta" } else { $null }

# the part of the pack a game file belongs to (package\build_package.py has the same rule)
function Get-Part([string]$rel) {
    $r = $rel.ToLower()
    if ($r -eq "ron_revived_version.txt" -or $r -eq "ron_revived_parts.txt") { return "meta" }
    if ($r.StartsWith("mod_override\sounds\tracks\")) { return "music" }
    if ($r.StartsWith("nation_art\") -or $r.StartsWith("mod_override\art\")) { return "art" }
    return "core"
}

$manifestName = "RON_Revived_files.txt"
$backupName = "RON_Revived_backup"

function Find-GameDir {
    $steam = (Get-ItemProperty -ErrorAction SilentlyContinue "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam").InstallPath
    if (-not $steam) { $steam = (Get-ItemProperty -ErrorAction SilentlyContinue "HKCU:\SOFTWARE\Valve\Steam").SteamPath }
    $libs = @()
    if ($steam) {
        $libs += $steam
        $vdf = Join-Path $steam "steamapps\libraryfolders.vdf"
        if (Test-Path $vdf) {
            foreach ($m in [regex]::Matches((Get-Content -Raw $vdf), '"path"\s+"([^"]+)"')) {
                $libs += $m.Groups[1].Value -replace '\\\\', '\'
            }
        }
    }
    foreach ($lib in $libs | Select-Object -Unique) {
        $dir = Join-Path $lib "steamapps\common\Rise of Nations"
        if (Test-Path (Join-Path $dir "riseofnations.exe")) { return $dir }
    }
    return $null
}

if (-not (Test-Path $payload)) {
    Write-Host "The game\ folder is missing next to install.bat."
    Write-Host "Extract the whole zip first (right-click, Extract All), then run install.bat from there."
    exit 1
}

if (-not $GameDir) { $GameDir = Find-GameDir }
if (-not $GameDir) {
    Write-Host "Rise of Nations was not found through Steam."
    $GameDir = Read-Host "Paste the game folder (the one with riseofnations.exe)"
}
$GameDir = $GameDir.Trim('"', ' ')
if (-not (Test-Path (Join-Path $GameDir "riseofnations.exe"))) {
    Write-Host "No riseofnations.exe in: $GameDir"
    exit 1
}

if (Get-Process -ErrorAction SilentlyContinue riseofnations) {
    Write-Host "Rise of Nations is running. Close the game, then run install.bat again."
    exit 1
}

$manifest = Join-Path $GameDir $manifestName
$backup = Join-Path $GameDir $backupName
$old = @{}
if (Test-Path $manifest) {
    foreach ($l in Get-Content $manifest) { if ($l.Trim()) { $old[$l.Trim().ToLower()] = $l.Trim() } }
}

# a winmm.dll that is neither ours nor from the older Smooth Camera: another mod
$dll = Join-Path $GameDir "winmm.dll"
if ((Test-Path $dll) -and -not $old.ContainsKey("winmm.dll") -and -not (Test-Path (Join-Path $GameDir "ron_camera.ini"))) {
    Write-Host "$GameDir already has a winmm.dll from another mod."
    Write-Host "RON Revived needs that file name too. Remove the other mod first; nothing was changed."
    exit 1
}

$smoothCamera = (Test-Path (Join-Path $GameDir "ron_camera.ini")) -and -not (Test-Path $manifest)
$files = Get-ChildItem -Recurse -File $payload | ForEach-Object { $_.FullName.Substring($payload.Length + 1) }
$new = @{}
$installed = New-Object System.Collections.Generic.List[string]
$kept = $false

foreach ($rel in $files) {
    $src = Join-Path $payload $rel
    $dst = Join-Path $GameDir $rel
    $new[$rel.ToLower()] = $rel

    # your settings stay; the new defaults go next to them for comparison
    if ($rel -ieq "ron_camera.ini" -and (Test-Path $dst)) {
        Copy-Item -Force $src (Join-Path $GameDir "ron_camera.default.ini")
        $installed.Add("ron_camera.default.ini")
        $new["ron_camera.default.ini"] = "ron_camera.default.ini"
        $kept = $true
        continue
    }

    # a game file (or another mod's) that is not from an earlier RON Revived install: keep a copy
    # (not the winmm.dll of the older Smooth Camera, which this replaces for good)
    $ours = $old.ContainsKey($rel.ToLower()) -or ($smoothCamera -and $rel -ieq "winmm.dll")
    if ((Test-Path $dst) -and -not $ours) {
        $bak = Join-Path $backup $rel
        if (-not (Test-Path $bak)) {
            New-Item -ItemType Directory -Force (Split-Path $bak) | Out-Null
            Copy-Item $dst $bak
        }
    }
    New-Item -ItemType Directory -Force (Split-Path $dst) | Out-Null
    Copy-Item -Force $src $dst
    $installed.Add($rel)
}

# files of an earlier version that this one no longer has (an update of some parts keeps the
# files of the others)
foreach ($key in $old.Keys) {
    if ($new.ContainsKey($key)) { continue }
    $rel = $old[$key]
    if ($only -and $only -notcontains (Get-Part $rel)) {
        $installed.Add($rel)
        continue
    }
    Remove-Item -ErrorAction SilentlyContinue (Join-Path $GameDir $rel)
    $bak = Join-Path $backup $rel
    if (Test-Path $bak) { Move-Item -Force $bak (Join-Path $GameDir $rel) }
}

# the 4 GB (large address aware) flag: the 32-bit game otherwise has only 2 GB, and long games on
# big maps run out of memory ("Could not create texture: The GPU device instance has been
# suspended"). Set in place, so a Steam update never gets an old exe back; uninstall clears it
# only when this set it.
$exe = Join-Path $GameDir "riseofnations.exe"
$bytes = [System.IO.File]::ReadAllBytes($exe)
$pe = [BitConverter]::ToInt32($bytes, 0x3C)
$laa = $false
if (-not ($bytes[$pe + 22] -band 0x20)) {
    $bytes[$pe + 22] = $bytes[$pe + 22] -bor 0x20
    [System.IO.File]::WriteAllBytes($exe, $bytes)
    Set-Content -Encoding ASCII (Join-Path $GameDir "RON_Revived_laa.txt") "riseofnations.exe: 4 GB flag set by RON Revived"
    $laa = $true
}

# ron_camera.ini is ours even when it was kept
if ($kept -and -not $installed.Contains("ron_camera.ini")) { $installed.Add("ron_camera.ini") }
Set-Content -Encoding ASCII $manifest $installed

Write-Host ""
Write-Host "RON Revived installed in: $GameDir"
Write-Host "  $($installed.Count) files."
if (Test-Path $backup) { Write-Host "  Game files it replaced are kept in $backupName\ (uninstall.bat puts them back)." }
if ($kept) { Write-Host "  Your ron_camera.ini was kept; the new defaults are in ron_camera.default.ini." }
if ($laa) { Write-Host "  riseofnations.exe can now use 4 GB of memory (run install.bat again after a Steam update or file check)." }
Write-Host "Start the game from Steam as usual."
