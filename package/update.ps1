# Updates RON Revived from its latest GitHub release. Installed in the game folder as
# RON_Revived_update.ps1; the launcher's Update button (winmm.dll, launcher_update.c) starts it
# and closes. It waits for the launcher to exit, downloads the parts of the pack that changed
# (RON_Revived_update.txt lists them with their hashes; RON_Revived_parts.txt holds the ones
# installed), checks their SHA-256, installs them with RON_Revived_install.ps1 and starts the
# game again through Steam.
param([string]$Index, [int]$WaitPid = 0)

$ErrorActionPreference = "Stop"
$game = $PSScriptRoot
$stage = Join-Path $env:TEMP "RON_Revived_update"
$Host.UI.RawUI.WindowTitle = "RON Revived update"
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Stop-Update([string]$msg) {
    Write-Host ""
    Write-Host $msg -ForegroundColor Red
    Write-Host "The game was not changed by this, or only in part: download the whole pack from"
    Write-Host "https://github.com/anastasescucristian93-max/RON-Revived/releases/latest and run install.bat."
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $stage
    Read-Host "Press Enter to close"
    exit 1
}

function Get-Url([string]$url, [string]$out, [string]$label) {
    $req = [Net.HttpWebRequest]::Create($url)
    $req.UserAgent = "RON-Revived-updater"
    $resp = $req.GetResponse()
    try {
        $total = $resp.ContentLength
        $in = $resp.GetResponseStream()
        $fs = [IO.File]::Create($out)
        try {
            $buf = New-Object byte[] 262144
            $done = 0
            $shown = -1
            while (($n = $in.Read($buf, 0, $buf.Length)) -gt 0) {
                $fs.Write($buf, 0, $n)
                $done += $n
                $mb = [math]::Floor($done / 1MB)
                if ($mb -ne $shown) {
                    $shown = $mb
                    if ($total -gt 0) { Write-Host -NoNewline ("`r  {0}: {1} of {2} MB   " -f $label, $mb, [math]::Ceiling($total / 1MB)) }
                    else { Write-Host -NoNewline ("`r  {0}: {1} MB   " -f $label, $mb) }
                }
            }
        } finally { $fs.Close(); $in.Close() }
    } finally { $resp.Close() }
    Write-Host ""
}

function Read-Pairs([string]$text) {
    $h = @{}
    foreach ($l in $text -split "`r?`n") {
        if ($l -match '^\s*([A-Za-z]+)\s*=\s*(.+?)\s*$') { $h[$Matches[1].ToLower()] = $Matches[2] }
    }
    return $h
}

Write-Host "RON Revived update"
Write-Host "------------------"

if ($WaitPid) {
    Write-Host "Waiting for the launcher to close..."
    try { [void](Get-Process -Id $WaitPid -ErrorAction Stop).WaitForExit(30000) } catch {}
}
foreach ($i in 1..30) {
    if (-not (Get-Process -ErrorAction SilentlyContinue riseofnations, patriots)) { break }
    if ($i -eq 1) { Write-Host "Close Rise of Nations to go on..." }
    Start-Sleep -Seconds 2
}
if (Get-Process -ErrorAction SilentlyContinue riseofnations, patriots) { Stop-Update "Rise of Nations is still running." }

# what is out, and what is here
try {
    $wc = New-Object Net.WebClient
    $wc.Headers.Add("User-Agent", "RON-Revived-updater")
    $list = Read-Pairs $wc.DownloadString($Index)
} catch { Stop-Update "Could not read the update list ($Index): $($_.Exception.Message)" }
$version = $list["version"]
$base = $list["base"]
if (-not $version -or -not $base) { Stop-Update "The update list has no version or no download address." }
$have = @{}
$partsFile = Join-Path $game "RON_Revived_parts.txt"
if (Test-Path $partsFile) { $have = Read-Pairs (Get-Content -Raw $partsFile) }

$parts = @()
$all = [ordered]@{}
foreach ($name in "core", "art", "music") {
    if (-not $list.Contains($name)) { continue }
    $f = $list[$name] -split '\s+'
    if ($f.Count -lt 4) { Stop-Update "The update list's line for $name is not complete." }
    $all[$name] = $f[0]
    if ($have[$name] -ne $f[0]) { $parts += , @($name, $f[1], [double]$f[2], $f[3]) }
}
Write-Host "Updating to $version."

Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $stage
$dl = Join-Path $stage "download"
$pack = Join-Path $stage "pack"
New-Item -ItemType Directory -Force $dl, (Join-Path $pack "game") | Out-Null

foreach ($p in $parts) {
    $name, $sha, $size, $file = $p
    $zip = Join-Path $dl $file
    try { Get-Url ($base + $file) $zip $name } catch { Stop-Update "Could not download $file : $($_.Exception.Message)" }
    if ((Get-FileHash -Algorithm SHA256 $zip).Hash -ne $sha.ToUpper()) { Stop-Update "$file did not arrive whole (its SHA-256 is wrong)." }
    try { [IO.Compression.ZipFile]::ExtractToDirectory($zip, $pack) } catch { Stop-Update "Could not unpack $file : $($_.Exception.Message)" }
    Remove-Item $zip
}

# the version and the parts it is made of, for the next check
Set-Content -Encoding ASCII (Join-Path $pack "game\RON_Revived_version.txt") $version
Set-Content -Encoding ASCII (Join-Path $pack "game\RON_Revived_parts.txt") (@("[parts]") + @($all.Keys | ForEach-Object { "$_=$($all[$_])" }))

# the new installer when the core part came with one, else the one installed
$installer = Join-Path $pack "game\RON_Revived_install.ps1"
if (-not (Test-Path $installer)) { $installer = Join-Path $game "RON_Revived_install.ps1" }
$names = ($parts | ForEach-Object { $_[0] }) -join ","
if (-not $names) { $names = "none" }
Write-Host "Installing..."
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $installer -GameDir $game -Payload (Join-Path $pack "game") -Parts $names
if ($LASTEXITCODE -ne 0) { Stop-Update "The install did not finish." }
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $stage

Write-Host ""
Write-Host "RON Revived $version is installed. Starting the game..." -ForegroundColor Green
Start-Process "steam://rungameid/287450"
Start-Sleep -Seconds 4
