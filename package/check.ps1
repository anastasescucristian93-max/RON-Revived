# RON Revived check: one report of everything that decides whether the mod loads, for a player
# whose game shows no sign of it. check.bat runs it; the report goes to the Desktop
# (RON_Revived_check.txt) and opens in Notepad, for the player to send along with the bug report.
# It changes nothing in the game or in Windows. Its last step loads winmm.dll from the game folder
# into a 32-bit PowerShell, the way the game does, to see whether Windows lets it load.
# -Report <file> and -NoWait (no prompt, no Notepad) are for testing.
param([string]$GameDir, [string]$Report, [switch]$NoWait)

$ErrorActionPreference = "Continue"
$out = New-Object System.Collections.Generic.List[string]
function Say([string]$s) { $out.Add($s); Write-Host $s }
function Section([string]$s) { Say ""; Say "== $s" }
function Try-Say([string]$label, [scriptblock]$b) {
    try { Say ("{0}: {1}" -f $label, (& $b)) } catch { Say ("{0}: (could not check: {1})" -f $label, $_.Exception.Message) }
}

# expected builds (camera-smooth\src\ron_camera.c EXE_TIMESTAMP, ron.h LAUNCHER_TIMESTAMP)
$exeStamp = 0x6674863F
$launcherStamp = 0x597669ED
$app = "287450"

function Get-PeStamp([string]$p) {
    $fs = [IO.File]::OpenRead($p)
    try {
        $b = New-Object byte[] 1024
        [void]$fs.Read($b, 0, 1024)
        $pe = [BitConverter]::ToInt32($b, 0x3C)
        $fs.Position = $pe + 8
        $t = New-Object byte[] 4
        [void]$fs.Read($t, 0, 4)
        return [BitConverter]::ToUInt32($t, 0)
    } finally { $fs.Close() }
}

function Get-SteamDir {
    $s = (Get-ItemProperty -ErrorAction SilentlyContinue "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam").InstallPath
    if (-not $s) { $s = (Get-ItemProperty -ErrorAction SilentlyContinue "HKCU:\SOFTWARE\Valve\Steam").SteamPath }
    return $s
}

Say "RON Revived check, $(Get-Date -Format 'yyyy-MM-dd HH:mm')"

Section "Windows"
Try-Say "Windows" { $o = Get-CimInstance Win32_OperatingSystem; "$($o.Caption) $($o.Version) ($($o.OSArchitecture))" }
Try-Say "Code page" { (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\Nls\CodePage").ACP }
Try-Say "Smart App Control" {
    $v = (Get-ItemProperty -ErrorAction Stop "HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy").VerifiedAndReputablePolicyState
    switch ($v) { 0 { "off" } 1 { "ON (can block the mod's DLL)" } 2 { "evaluation" } default { "$v" } }
}
Try-Say "Antivirus" { (Get-CimInstance -Namespace root/SecurityCenter2 -ClassName AntiVirusProduct -ErrorAction Stop | ForEach-Object { $_.displayName }) -join ", " }
Try-Say "Defender detections of the mod" {
    $d = Get-MpThreatDetection -ErrorAction Stop | Where-Object { ($_.Resources -join " ") -match "Rise of Nations|winmm|RON_Revived" }
    if ($d) { ($d | ForEach-Object { "$($_.InitialDetectionTime) $($_.Resources -join ' ')" }) -join " | " } else { "none" }
}
Try-Say "winmm.dll in KnownDLLs" {
    $k = Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs"
    if ($k.PSObject.Properties | Where-Object { "$($_.Value)" -ieq "winmm.dll" }) { "YES (Windows will always use its own)" } else { "no" }
}

Section "Steam"
$steam = Get-SteamDir
Say "Steam: $steam"
$found = @()
if ($steam) {
    $libs = @($steam)
    $vdf = Join-Path $steam "steamapps\libraryfolders.vdf"
    if (Test-Path $vdf) {
        foreach ($m in [regex]::Matches((Get-Content -Raw $vdf), '"path"\s+"([^"]+)"')) { $libs += $m.Groups[1].Value -replace '\\\\', '\' }
    }
    foreach ($lib in $libs | Select-Object -Unique) {
        $dir = Join-Path $lib "steamapps\common\Rise of Nations"
        $has = Test-Path (Join-Path $dir "riseofnations.exe")
        Say ("Library {0}: {1}" -f $lib, $(if ($has) { "has Rise of Nations" } else { "-" }))
        if ($has) { $found += $dir }
    }
    # per-user settings: the overlay (the launcher gets the mod only through it) and launch options
    foreach ($lc in Get-ChildItem -ErrorAction SilentlyContinue (Join-Path $steam "userdata\*\config\localconfig.vdf")) {
        $t = Get-Content -Raw $lc.FullName
        $user = $lc.Directory.Parent.Name
        $g = [regex]::Match($t, '"EnableGameOverlay"\s+"(\d)"')
        Say ("User {0}: overlay for all games {1}" -f $user, $(if ($g.Success) { if ($g.Groups[1].Value -eq "0") { "OFF" } else { "on" } } else { "on (default)" }))
        $i = $t.IndexOf("`"$app`"")
        while ($i -ge 0) {
            $open = $t.IndexOf("{", $i)
            $gap = if ($open -gt $i) { $t.Substring($i + $app.Length + 2, $open - $i - $app.Length - 2) } else { "x" }
            if ($open -gt 0 -and $gap.Trim() -eq "") {
                $depth = 0; $end = $open
                for ($j = $open; $j -lt $t.Length; $j++) {
                    if ($t[$j] -eq "{") { $depth++ } elseif ($t[$j] -eq "}") { $depth--; if ($depth -eq 0) { $end = $j; break } }
                }
                $block = $t.Substring($open, $end - $open + 1)
                $ov = [regex]::Match($block, '"OverlayAppEnable"\s+"(\d)"')
                $lo = [regex]::Match($block, '"LaunchOptions"\s+"([^"]*)"')
                if ($ov.Success -or $lo.Success) {
                    Say ("User {0}: Rise of Nations overlay {1}, launch options '{2}'" -f $user,
                        $(if ($ov.Success) { if ($ov.Groups[1].Value -eq "0") { "OFF" } else { "on" } } else { "on (default)" }),
                        $(if ($lo.Success) { $lo.Groups[1].Value } else { "" }))
                }
            }
            $i = $t.IndexOf("`"$app`"", $i + 1)
        }
    }
}

if (-not $GameDir) { $GameDir = $found | Select-Object -First 1 }
if (-not $GameDir) {
    Write-Host ""
    $GameDir = (Read-Host "Rise of Nations was not found through Steam. Paste the game folder (the one with riseofnations.exe)").Trim('"', ' ')
}

Section "Game folder"
Say "Folder: $GameDir"
Try-Say "Path usable by the mod" {
    $acp = [int](Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\Nls\CodePage").ACP
    if ($acp -eq 65001) { return "yes (UTF-8 code page)" }
    $enc = [Text.Encoding]::GetEncoding($acp)
    if ($enc.GetString($enc.GetBytes($GameDir)) -eq $GameDir) { return "yes" }
    $short = (New-Object -ComObject Scripting.FileSystemObject).GetFolder($GameDir).ShortPath
    if ($short -and $enc.GetString($enc.GetBytes($short)) -eq $short) { "through its short name $short" } else { "NO: special letters and no short name" }
}
foreach ($f in @("riseofnations.exe", "patriots.exe")) {
    $p = Join-Path $GameDir $f
    if (-not (Test-Path $p)) { Say "${f}: MISSING"; continue }
    Try-Say $f {
        $s = Get-PeStamp $p
        $want = if ($f -eq "patriots.exe") { $launcherStamp } else { $exeStamp }
        "{0:N0} bytes, build {1:X8} ({2})" -f (Get-Item $p).Length, $s, $(if ($s -eq $want) { "the supported one" } else { "NOT the supported build" })
    }
}
Try-Say "Compatibility settings" {
    $hits = foreach ($k in "HKCU:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers", "HKLM:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers") {
        $item = Get-ItemProperty -ErrorAction SilentlyContinue $k
        if ($item) { $item.PSObject.Properties | Where-Object { $_.Name -match "riseofnations|patriots" } | ForEach-Object { "$($_.Name) = $($_.Value)" } }
    }
    if ($hits) { $hits -join " | " } else { "none" }
}
Try-Say "Exploit protection (riseofnations.exe)" {
    $sys = Get-ProcessMitigation -System -ErrorAction Stop
    $w = "system-wide: prefer System32 = $($sys.ImageLoad.PreferSystem32), signed only = $($sys.BinarySignature.MicrosoftSignedOnly)"
    $m = Get-ProcessMitigation -Name riseofnations.exe -ErrorAction Stop
    if (-not $m) { return "none for the game; $w" }
    "$w; for the game: image load: prefer System32 = $($m.ImageLoad.PreferSystem32), block remote = $($m.ImageLoad.BlockRemoteImageLoads), block low label = $($m.ImageLoad.BlockLowLabelImageLoads); signed only = $($m.BinarySignature.MicrosoftSignedOnly)"
}
Try-Say "Image File Execution Options" {
    $k = foreach ($n in "riseofnations.exe", "patriots.exe") { Get-Item -ErrorAction SilentlyContinue "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\$n" }
    if ($k) { ($k | ForEach-Object { "$($_.PSChildName): $($_.Property -join ', ')" }) -join " | " } else { "none" }
}

$dll = Join-Path $GameDir "winmm.dll"
if (Test-Path $dll) {
    Try-Say "winmm.dll" { $i = Get-Item $dll; "{0:N0} bytes, {1:yyyy-MM-dd HH:mm}, SHA-256 {2}" -f $i.Length, $i.LastWriteTime, (Get-FileHash $dll).Hash.Substring(0, 16) }
    Try-Say "winmm.dll marked as downloaded" { if (Get-Item -ErrorAction SilentlyContinue -Stream Zone.Identifier $dll) { "yes" } else { "no" } }
} else {
    Say "winmm.dll: MISSING (the mod cannot load without it)"
}
Try-Say "Version" { $v = Join-Path $GameDir "RON_Revived_version.txt"; if (Test-Path $v) { (Get-Content $v -TotalCount 1).Trim() } else { "no version file" } }
Try-Say "Installed files" {
    $m = Join-Path $GameDir "RON_Revived_files.txt"
    if (-not (Test-Path $m)) { return "no list (install.bat did not finish here)" }
    $all = @(Get-Content $m | Where-Object { $_.Trim() })
    $miss = @($all | Where-Object { -not (Test-Path (Join-Path $GameDir $_.Trim())) })
    "$($all.Count) listed, $($miss.Count) missing" + $(if ($miss) { ": " + (($miss | Select-Object -First 5) -join ", ") } else { "" })
}
Try-Say "Other mods in the folder" {
    $x = Get-ChildItem -ErrorAction SilentlyContinue $GameDir -File | Where-Object { $_.Name -match '^(dinput8|d3d9|d3d11|dxgi|version|dsound)\.dll$' } | ForEach-Object { $_.Name }
    if ($x) { $x -join ", " } else { "none" }
}

# the logs, before the load test below starts a new one
foreach ($l in @((Join-Path $GameDir "ron_camera.log"), (Join-Path $GameDir "ron_camera.old.log"), (Join-Path $env:LOCALAPPDATA "RON_Revived\ron_camera.log"))) {
    Section $l
    if (Test-Path $l) {
        $i = Get-Item $l
        Say ("{0:yyyy-MM-dd HH:mm}, last 25 lines:" -f $i.LastWriteTime)
        Get-Content $l -Tail 25 | ForEach-Object { Say "  $_" }
    } else { Say "not there" }
}

# the running game: which winmm.dll it really loaded (a 32-bit process; only a 32-bit
# PowerShell sees its DLLs)
$ps32 = Join-Path $env:WINDIR "SysWOW64\WindowsPowerShell\v1.0\powershell.exe"
Section "Running game"
if (-not $NoWait -and -not (Get-Process -ErrorAction SilentlyContinue riseofnations, patriots)) {
    Write-Host ""
    Write-Host "Start Rise of Nations from Steam now and wait for the launcher (or the game's main menu),"
    Read-Host "then come back here and press Enter (or just press Enter to skip)" | Out-Null
}
$procs = Get-Process -ErrorAction SilentlyContinue riseofnations, patriots
if ($procs) {
    foreach ($p in $procs) {
        $mods = & $ps32 -NoProfile -Command "(Get-Process -Id $($p.Id)).Modules | Where-Object { `$_.ModuleName -match '^(winmm|gameoverlayrenderer)\.dll$' } | ForEach-Object { `$_.FileName }"
        Say ("{0}.exe: {1}" -f $p.ProcessName, $(if ($mods) { ($mods -join " | ") } else { "no winmm.dll / overlay loaded" }))
    }
    Say "(the mod is loaded when winmm.dll is the one in the game folder, not System32 / SysWOW64)"
} else { Say "the game was not running" }

# the load test: the game's way (LoadLibrary of the folder's winmm.dll in a 32-bit process)
Section "Load test"
if (Test-Path $dll) {
    $test = Join-Path $env:TEMP "RON_Revived_loadtest.ps1"
    Set-Content -Encoding UTF8 $test @'
param($dll)
Add-Type -Namespace W -Name K -MemberDefinition '[DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] public static extern System.IntPtr LoadLibraryW(string p);'
$h = [W.K]::LoadLibraryW($dll)
$e = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
if ($h -ne [IntPtr]::Zero) { "loaded" } else { "REFUSED, error ${e}: " + (New-Object ComponentModel.Win32Exception $e).Message }
'@
    $r = & $ps32 -NoProfile -ExecutionPolicy Bypass -File $test $dll 2>&1
    Remove-Item -ErrorAction SilentlyContinue $test
    Say "winmm.dll: $r"
    $log = Join-Path $GameDir "ron_camera.log"
    Say ("ron_camera.log after the test: {0}" -f $(if ((Test-Path $log) -and (Get-Item $log).LastWriteTime -gt (Get-Date).AddMinutes(-2)) { "written (the mod can read and write its folder)" } else { "NOT written" }))
} else { Say "skipped, no winmm.dll" }

$report = if ($Report) { $Report } else { Join-Path ([Environment]::GetFolderPath("Desktop")) "RON_Revived_check.txt" }
Set-Content -Encoding UTF8 $report $out
Write-Host ""
Write-Host "Report saved: $report"
Write-Host "Send this file with your bug report (Discord, GitHub or Nexus)."
if (-not $NoWait) { Start-Process notepad.exe $report }
