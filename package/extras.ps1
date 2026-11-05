# Installs or removes one extra of RON Revived (an optional part, such as the nation music).
# Installed in the game folder as RON_Revived_extras.ps1; the launcher's Extras tray (winmm.dll,
# launcher_extras.c) downloads the extra's zips, checks them and runs this hidden:
#   -Action install -Part <id> -Zips "<zip>;<zip>" -Content <content hash>
#   -Action remove  -Part <id> (-Zips and -Content "none")
# The files go in through RON_Revived_install.ps1 (backups and RON_Revived_files.txt as for the
# pack); RON_Revived_parts.txt gets the extra's content hash, which the tray and the updater read.
# What it did goes to %TEMP%\RON_Revived_extras.log. Exit code 0 when it worked.
param([string]$Action, [string]$Part, [string]$Zips, [string]$Content)

$ErrorActionPreference = "Stop"
$game = $PSScriptRoot
$log = Join-Path $env:TEMP "RON_Revived_extras.log"
$installer = Join-Path $game "RON_Revived_install.ps1"
$partsFile = Join-Path $game "RON_Revived_parts.txt"

function Write-Log([string]$msg) {
    Add-Content -Encoding UTF8 $log ("{0:yyyy-MM-dd HH:mm:ss} {1}" -f (Get-Date), $msg)
}

try {
    $Part = $Part.ToLower()
    if ($Part -notmatch '^[a-z0-9]+$') { throw "not an extra: $Part" }
    if (-not (Test-Path $installer)) { throw "$installer is missing" }
    Write-Log "$Action $Part"

    if ($Action -eq "install") {
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $stage = Join-Path $env:TEMP "RON_Revived_extras\pack"
        Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $stage
        New-Item -ItemType Directory -Force $stage | Out-Null
        foreach ($zip in $Zips -split ';') {
            if (-not $zip -or $zip -eq "none") { continue }
            [IO.Compression.ZipFile]::ExtractToDirectory($zip, $stage)
            Remove-Item -ErrorAction SilentlyContinue $zip
        }
        if (-not (Test-Path (Join-Path $stage "game"))) { throw "the zips held no game\ folder" }
        $out = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $installer -GameDir $game -Payload (Join-Path $stage "game") -Parts $Part 2>&1
        $code = $LASTEXITCODE
        Write-Log ($out -join " | ")
        Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $stage
        if ($code -ne 0) { throw "the install ended with $code" }
        # the extra is there: its content hash, for the tray and the updater
        $lines = @()
        if (Test-Path $partsFile) { $lines = @(Get-Content $partsFile | Where-Object { $_ -notmatch "^\s*$Part\s*=" }) }
        if (-not ($lines -match '^\s*\[parts\]')) { $lines = @("[parts]") + $lines }
        Set-Content -Encoding ASCII $partsFile ($lines + "$Part=$Content")
    } elseif ($Action -eq "remove") {
        $out = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $installer -GameDir $game -Remove $Part 2>&1
        $code = $LASTEXITCODE
        Write-Log ($out -join " | ")
        if ($code -ne 0) { throw "the removal ended with $code" }
    } else {
        throw "no such action: $Action"
    }
    Write-Log "$Action $Part done"
    exit 0
} catch {
    Write-Log "FAILED: $($_.Exception.Message)"
    exit 1
}
