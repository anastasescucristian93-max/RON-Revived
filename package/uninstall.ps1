# Uninstalls RON Revived: removes the files listed in RON_Revived_files.txt and puts back the game
# files kept in RON_Revived_backup\.
param([string]$GameDir)

$ErrorActionPreference = "Stop"
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

if (-not $GameDir) { $GameDir = Find-GameDir }
if (-not $GameDir) {
    Write-Host "Rise of Nations was not found through Steam."
    $GameDir = Read-Host "Paste the game folder (the one with riseofnations.exe)"
}
$GameDir = $GameDir.Trim('"', ' ')

$manifest = Join-Path $GameDir $manifestName
$backup = Join-Path $GameDir $backupName
if (-not (Test-Path $manifest)) {
    Write-Host "RON Revived does not seem to be installed in $GameDir (no $manifestName)."
    exit 0
}
if (Get-Process -ErrorAction SilentlyContinue riseofnations) {
    Write-Host "Rise of Nations is running. Close the game, then run uninstall.bat again."
    exit 1
}

$n = 0
foreach ($rel in Get-Content $manifest) {
    $rel = $rel.Trim()
    if (-not $rel) { continue }
    Remove-Item -ErrorAction SilentlyContinue (Join-Path $GameDir $rel)
    $n++
}
foreach ($f in "ron_camera.log", "ron_camera.old.log", "ron_zoom_trace.csv") {
    Remove-Item -ErrorAction SilentlyContinue (Join-Path $GameDir $f)
}

# the game files it had replaced
$restored = 0
if (Test-Path $backup) {
    foreach ($f in Get-ChildItem -Recurse -File $backup) {
        $rel = $f.FullName.Substring($backup.Length + 1)
        $dst = Join-Path $GameDir $rel
        New-Item -ItemType Directory -Force (Split-Path $dst) | Out-Null
        Move-Item -Force $f.FullName $dst
        $restored++
    }
    Remove-Item -Recurse -Force $backup
}
Remove-Item $manifest

# the 4 GB flag, only when install.ps1 set it
$laaMark = Join-Path $GameDir "RON_Revived_laa.txt"
if (Test-Path $laaMark) {
    $exe = Join-Path $GameDir "riseofnations.exe"
    $bytes = [System.IO.File]::ReadAllBytes($exe)
    $pe = [BitConverter]::ToInt32($bytes, 0x3C)
    if ($bytes[$pe + 22] -band 0x20) {
        $bytes[$pe + 22] = $bytes[$pe + 22] -band 0xDF
        [System.IO.File]::WriteAllBytes($exe, $bytes)
    }
    Remove-Item $laaMark
}

# folders left empty (deepest first)
foreach ($top in "mod_override", "nation_art") {
    $dir = Join-Path $GameDir $top
    if (-not (Test-Path $dir)) { continue }
    Get-ChildItem -Recurse -Directory $dir | Sort-Object { $_.FullName.Length } -Descending | ForEach-Object {
        if (-not (Get-ChildItem -Force $_.FullName)) { Remove-Item $_.FullName }
    }
    if (-not (Get-ChildItem -Force $dir)) { Remove-Item $dir }
}
# our game scripts' folders (the game's own scenario\ stays)
foreach ($s in "scenario\Scripts\Survival") {
    $dir = Join-Path $GameDir $s
    if ((Test-Path $dir) -and -not (Get-ChildItem -Force $dir)) { Remove-Item $dir }
}

Write-Host "RON Revived uninstalled from: $GameDir"
Write-Host "  $n files removed, $restored game files put back."
