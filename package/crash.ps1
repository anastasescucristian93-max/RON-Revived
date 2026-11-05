# The crash panel of RON Revived. Installed in the game folder as RON_Revived_crash.ps1;
# winmm.dll (camera-smooth\src\crash.c) starts it when the game dies on an exception, with the
# report it wrote (crash\crash_<date>_<time>.txt) and a minidump next to it. The player can add
# what they were doing and send both to the RON Revived Discord through a webhook.
# package\build_package.py puts the webhook address in place of the marker below
# (package\crash_webhook.txt, not in the repository).
param([string]$Report, [string]$Dump = "", [int]$WaitPid = 0)

$Webhook = "__RON_REVIVED_CRASH_WEBHOOK__"
$ErrorActionPreference = "Stop"
$game = $PSScriptRoot

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Net.Http
Add-Type -AssemblyName System.IO.Compression.FileSystem
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
[Windows.Forms.Application]::EnableVisualStyles()

if ($WaitPid -gt 0) {
    try { (Get-Process -Id $WaitPid -ErrorAction Stop).WaitForExit(10000) | Out-Null } catch {}
}
if (-not $Report -or -not (Test-Path -LiteralPath $Report)) { exit 1 }

# ---- the game's language (Options > Player > Language, or the launcher's flag) ----------------
$lang = "EN"
try {
    $ini = Join-Path $env:APPDATA "Microsoft Games\Rise of Nations\rise2.ini"
    $m = Select-String -LiteralPath $ini -Pattern '^\s*Language\s*=\s*([A-Za-z]+)' | Select-Object -First 1
    if ($m) { $lang = $m.Matches[0].Groups[1].Value.ToUpper() }
} catch {}

$T = @{
    EN = @{ title = "RON Revived - the game crashed"; head = "Rise of Nations closed because of an error."
            text = "You can send the crash report to the RON Revived Discord, so the cause can be found and fixed. It holds technical data only: where the game failed, the mod's log, your Windows version and memory, and a small memory dump of the game."
            what = "What were you doing when it happened? (optional)"; who = "Your Discord name (optional, so we can ask you more)"
            dump = "Include the memory dump (recommended)"; see = "See the report"; send = "Send report"; close = "Close"
            sending = "Sending..."; sent = "Sent. Thank you!"; failed = "Could not send it: "; nohook = "This version has no Discord address for reports. The report is in the crash folder." }
    RO = @{ title = "RON Revived - jocul s-a oprit"; head = "Rise of Nations s-a închis din cauza unei erori."
            text = "Poți trimite raportul pe Discordul RON Revived, ca să se găsească și să se repare cauza. Conține doar date tehnice: unde a căzut jocul, log-ul modului, versiunea de Windows și memoria, plus o mică imagine a memoriei jocului."
            what = "Ce făceai când s-a întâmplat? (opțional)"; who = "Numele tău pe Discord (opțional, ca să te putem întreba)"
            dump = "Include imaginea memoriei (recomandat)"; see = "Vezi raportul"; send = "Trimite raportul"; close = "Închide"
            sending = "Se trimite..."; sent = "Trimis. Mulțumim!"; failed = "Nu s-a putut trimite: "; nohook = "Versiunea aceasta nu are adresa de Discord pentru rapoarte. Raportul e în folderul crash." }
    DE = @{ title = "RON Revived - das Spiel ist abgestürzt"; head = "Rise of Nations wurde wegen eines Fehlers beendet."
            text = "Du kannst den Absturzbericht an den RON Revived Discord senden, damit die Ursache gefunden und behoben wird. Er enthält nur technische Daten: wo das Spiel fehlschlug, das Log der Mod, deine Windows-Version und den Speicher sowie einen kleinen Speicherauszug des Spiels."
            what = "Was hast du gerade gemacht? (optional)"; who = "Dein Discord-Name (optional, für Rückfragen)"
            dump = "Speicherauszug anhängen (empfohlen)"; see = "Bericht ansehen"; send = "Bericht senden"; close = "Schließen"
            sending = "Wird gesendet..."; sent = "Gesendet. Danke!"; failed = "Senden fehlgeschlagen: "; nohook = "Diese Version hat keine Discord-Adresse für Berichte. Der Bericht liegt im Ordner crash." }
    ES = @{ title = "RON Revived - el juego se cerró"; head = "Rise of Nations se cerró por un error."
            text = "Puedes enviar el informe al Discord de RON Revived para que se encuentre y corrija la causa. Solo contiene datos técnicos: dónde falló el juego, el registro del mod, tu versión de Windows y la memoria, y un pequeño volcado de memoria del juego."
            what = "¿Qué estabas haciendo cuando ocurrió? (opcional)"; who = "Tu nombre en Discord (opcional, para preguntarte)"
            dump = "Incluir el volcado de memoria (recomendado)"; see = "Ver el informe"; send = "Enviar informe"; close = "Cerrar"
            sending = "Enviando..."; sent = "Enviado. ¡Gracias!"; failed = "No se pudo enviar: "; nohook = "Esta versión no tiene dirección de Discord para informes. El informe está en la carpeta crash." }
    FR = @{ title = "RON Revived - le jeu a planté"; head = "Rise of Nations s'est fermé à cause d'une erreur."
            text = "Tu peux envoyer le rapport au Discord de RON Revived pour que la cause soit trouvée et corrigée. Il ne contient que des données techniques : où le jeu a échoué, le journal du mod, ta version de Windows et la mémoire, et un petit vidage mémoire du jeu."
            what = "Que faisais-tu à ce moment-là ? (facultatif)"; who = "Ton nom Discord (facultatif, pour te poser des questions)"
            dump = "Joindre le vidage mémoire (recommandé)"; see = "Voir le rapport"; send = "Envoyer le rapport"; close = "Fermer"
            sending = "Envoi..."; sent = "Envoyé. Merci !"; failed = "Envoi impossible : "; nohook = "Cette version n'a pas d'adresse Discord pour les rapports. Le rapport est dans le dossier crash." }
    IT = @{ title = "RON Revived - il gioco si è bloccato"; head = "Rise of Nations si è chiuso per un errore."
            text = "Puoi inviare il rapporto al Discord di RON Revived, così la causa può essere trovata e corretta. Contiene solo dati tecnici: dove il gioco si è fermato, il log della mod, la versione di Windows e la memoria, e un piccolo dump di memoria del gioco."
            what = "Cosa stavi facendo quando è successo? (facoltativo)"; who = "Il tuo nome Discord (facoltativo, per farti domande)"
            dump = "Includi il dump di memoria (consigliato)"; see = "Vedi il rapporto"; send = "Invia rapporto"; close = "Chiudi"
            sending = "Invio..."; sent = "Inviato. Grazie!"; failed = "Invio non riuscito: "; nohook = "Questa versione non ha un indirizzo Discord per i rapporti. Il rapporto è nella cartella crash." }
    RU = @{ title = "RON Revived - игра аварийно завершилась"; head = "Rise of Nations закрылась из-за ошибки."
            text = "Вы можете отправить отчёт на Discord RON Revived, чтобы причину нашли и исправили. В нём только технические данные: где произошёл сбой, журнал мода, версия Windows и память, а также небольшой дамп памяти игры."
            what = "Что вы делали в этот момент? (необязательно)"; who = "Ваше имя в Discord (необязательно, чтобы мы могли спросить)"
            dump = "Приложить дамп памяти (рекомендуется)"; see = "Показать отчёт"; send = "Отправить отчёт"; close = "Закрыть"
            sending = "Отправка..."; sent = "Отправлено. Спасибо!"; failed = "Не удалось отправить: "; nohook = "В этой версии нет адреса Discord для отчётов. Отчёт лежит в папке crash." }
    PT = @{ title = "RON Revived - o jogo travou"; head = "Rise of Nations fechou por causa de um erro."
            text = "Você pode enviar o relatório para o Discord do RON Revived, para que a causa seja encontrada e corrigida. Ele contém só dados técnicos: onde o jogo falhou, o log do mod, sua versão do Windows e a memória, e um pequeno dump de memória do jogo."
            what = "O que você estava fazendo quando aconteceu? (opcional)"; who = "Seu nome no Discord (opcional, para podermos perguntar)"
            dump = "Incluir o dump de memória (recomendado)"; see = "Ver o relatório"; send = "Enviar relatório"; close = "Fechar"
            sending = "Enviando..."; sent = "Enviado. Obrigado!"; failed = "Não foi possível enviar: "; nohook = "Esta versão não tem endereço do Discord para relatórios. O relatório está na pasta crash." }
    PL = @{ title = "RON Revived - gra uległa awarii"; head = "Rise of Nations zamknęło się z powodu błędu."
            text = "Możesz wysłać raport na Discord RON Revived, aby znaleźć i naprawić przyczynę. Zawiera tylko dane techniczne: gdzie gra się zatrzymała, log moda, wersję Windows i pamięć oraz mały zrzut pamięci gry."
            what = "Co robiłeś, gdy to się stało? (opcjonalnie)"; who = "Twoja nazwa na Discordzie (opcjonalnie, żebyśmy mogli dopytać)"
            dump = "Dołącz zrzut pamięci (zalecane)"; see = "Pokaż raport"; send = "Wyślij raport"; close = "Zamknij"
            sending = "Wysyłanie..."; sent = "Wysłano. Dziękujemy!"; failed = "Nie udało się wysłać: "; nohook = "Ta wersja nie ma adresu Discorda dla raportów. Raport jest w folderze crash." }
    TR = @{ title = "RON Revived - oyun çöktü"; head = "Rise of Nations bir hata nedeniyle kapandı."
            text = "Çökme raporunu RON Revived Discord'una gönderebilirsin; böylece neden bulunup düzeltilir. Yalnızca teknik veriler içerir: oyunun nerede çöktüğü, modun günlüğü, Windows sürümün ve bellek, ayrıca oyunun küçük bir bellek dökümü."
            what = "Bu olduğunda ne yapıyordun? (isteğe bağlı)"; who = "Discord adın (isteğe bağlı, sana sorabilmemiz için)"
            dump = "Bellek dökümünü ekle (önerilir)"; see = "Raporu göster"; send = "Raporu gönder"; close = "Kapat"
            sending = "Gönderiliyor..."; sent = "Gönderildi. Teşekkürler!"; failed = "Gönderilemedi: "; nohook = "Bu sürümde raporlar için Discord adresi yok. Rapor crash klasöründe." }
}
$S = $T[$lang]
if (-not $S) { $S = $T["EN"] }

# ---- the report, without the Windows user name ------------------------------------------------
function Get-Clean([string]$text) {
    foreach ($v in @($env:USERPROFILE, $env:USERNAME, $env:COMPUTERNAME)) {
        if ($v -and $v.Length -ge 3) { $text = $text -ireplace [regex]::Escape($v), "<user>" }
    }
    return $text
}
$reportText = Get-Clean ([IO.File]::ReadAllText($Report))
$version = "?"
$summary = @()
foreach ($l in $reportText -split "`r?`n") {
    if ($l -match '^RON Revived version:\s*(.+)$') { $version = $Matches[1] }
    if ($l -match '^(Exception|At|Access):') { $summary += $l }
}

# ---- the panel --------------------------------------------------------------------------------
$font = New-Object Drawing.Font("Segoe UI", 10)
$form = New-Object Windows.Forms.Form
$form.Text = $S.title
$form.Font = $font
$form.FormBorderStyle = "FixedDialog"
$form.MaximizeBox = $false
$form.MinimizeBox = $false
$form.StartPosition = "CenterScreen"
$form.TopMost = $true
$form.ClientSize = New-Object Drawing.Size(560, 470)
try { $form.Icon = [Drawing.Icon]::ExtractAssociatedIcon((Join-Path $game "riseofnations.exe")) } catch {}

function Add-Label([string]$text, [int]$y, [int]$h, [Drawing.Font]$f = $font) {
    $l = New-Object Windows.Forms.Label
    $l.Text = $text
    $l.Font = $f
    $l.Location = New-Object Drawing.Point(20, $y)
    $l.Size = New-Object Drawing.Size(520, $h)
    $form.Controls.Add($l)
    return $l
}

Add-Label $S.head 16 26 (New-Object Drawing.Font("Segoe UI", 12, [Drawing.FontStyle]::Bold)) | Out-Null
Add-Label $S.text 48 80 | Out-Null
Add-Label $S.what 134 22 | Out-Null
$what = New-Object Windows.Forms.TextBox
$what.Multiline = $true
$what.ScrollBars = "Vertical"
$what.Location = New-Object Drawing.Point(20, 158)
$what.Size = New-Object Drawing.Size(520, 110)
$what.MaxLength = 1200
$form.Controls.Add($what)
Add-Label $S.who 278 22 | Out-Null
$who = New-Object Windows.Forms.TextBox
$who.Location = New-Object Drawing.Point(20, 302)
$who.Size = New-Object Drawing.Size(300, 26)
$who.MaxLength = 40
$form.Controls.Add($who)

$haveDump = $Dump -and (Test-Path -LiteralPath $Dump)
$withDump = New-Object Windows.Forms.CheckBox
$withDump.Text = $S.dump
$withDump.Location = New-Object Drawing.Point(20, 340)
$withDump.Size = New-Object Drawing.Size(520, 24)
$withDump.Checked = $haveDump
$withDump.Enabled = $haveDump
$form.Controls.Add($withDump)

$see = New-Object Windows.Forms.LinkLabel
$see.Text = $S.see
$see.Location = New-Object Drawing.Point(20, 372)
$see.Size = New-Object Drawing.Size(300, 22)
$see.add_LinkClicked({ Start-Process notepad.exe -ArgumentList "`"$Report`"" })
$form.Controls.Add($see)

$status = Add-Label "" 400 22
$send = New-Object Windows.Forms.Button
$send.Text = $S.send
$send.Location = New-Object Drawing.Point(300, 428)
$send.Size = New-Object Drawing.Size(130, 30)
$close = New-Object Windows.Forms.Button
$close.Text = $S.close
$close.Location = New-Object Drawing.Point(440, 428)
$close.Size = New-Object Drawing.Size(100, 30)
$close.add_Click({ $form.Close() })
$form.Controls.Add($send)
$form.Controls.Add($close)
$form.AcceptButton = $send
$form.CancelButton = $close

if ($Webhook -notmatch '^https://(canary\.|ptb\.)?discord(app)?\.com/api/webhooks/') {
    $send.Enabled = $false
    $status.Text = $S.nohook
}

function Send-Report {
    $send.Enabled = $false
    $status.ForeColor = [Drawing.Color]::Black
    $status.Text = $S.sending
    $form.Refresh()
    $client = $null
    $zip = $null
    try {
        $name = $who.Text.Trim() -replace '[@`]', ''
        $note = $what.Text.Trim() -replace '@', '(at)'
        $msg = "**Crash** - RON Revived $version`n``````n" + ($summary -join "`n") + "`n``````"
        if ($name) { $msg += "`n**Player:** $name" }
        if ($note) { $msg += "`n**What they were doing:** $note" }
        if ($msg.Length -gt 1990) { $msg = $msg.Substring(0, 1990) }
        $payload = @{ username = "RON Revived crash"; content = $msg; allowed_mentions = @{ parse = @() } } |
            ConvertTo-Json -Depth 4 -Compress

        $form_data = New-Object Net.Http.MultipartFormDataContent
        $json = New-Object Net.Http.StringContent($payload, [Text.Encoding]::UTF8, "application/json")
        $form_data.Add($json, "payload_json")
        $bytes = [Text.Encoding]::UTF8.GetBytes($reportText)
        $form_data.Add((New-Object Net.Http.ByteArrayContent(, $bytes)), "files[0]", [IO.Path]::GetFileName($Report))
        if ($withDump.Checked -and $haveDump) {
            $zip = [IO.Path]::ChangeExtension($Dump, ".zip")
            Remove-Item -LiteralPath $zip -ErrorAction SilentlyContinue
            $za = [IO.Compression.ZipFile]::Open($zip, "Create")
            try {
                [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($za, $Dump, [IO.Path]::GetFileName($Dump), "Optimal") | Out-Null
            } finally { $za.Dispose() }
            if ((Get-Item -LiteralPath $zip).Length -lt 9.5MB) {
                $form_data.Add((New-Object Net.Http.ByteArrayContent(, [IO.File]::ReadAllBytes($zip))), "files[1]",
                    [IO.Path]::GetFileName($zip))
            }
        }
        $client = New-Object Net.Http.HttpClient
        $client.Timeout = [TimeSpan]::FromSeconds(60)
        $client.DefaultRequestHeaders.UserAgent.ParseAdd("RON-Revived-crash-report")
        $resp = $client.PostAsync($Webhook, $form_data).GetAwaiter().GetResult()
        if (-not $resp.IsSuccessStatusCode) {
            throw ("HTTP " + [int]$resp.StatusCode + " " + $resp.ReasonPhrase)
        }
        $status.ForeColor = [Drawing.Color]::DarkGreen
        $status.Text = $S.sent
        try { Rename-Item -LiteralPath $Report -NewName ([IO.Path]::GetFileNameWithoutExtension($Report) + ".sent.txt") } catch {}
    } catch {
        $status.ForeColor = [Drawing.Color]::DarkRed
        $status.Text = $S.failed + $_.Exception.Message
        $send.Enabled = $true
    } finally {
        if ($client) { $client.Dispose() }
        if ($zip) { Remove-Item -LiteralPath $zip -ErrorAction SilentlyContinue }
    }
}
$send.add_Click({ Send-Report })

# Keep the crash folder small: the newest 10 reports and their dumps.
try {
    Get-ChildItem -LiteralPath (Split-Path -Parent $Report) -Filter "crash_*" |
        Sort-Object LastWriteTime -Descending | Select-Object -Skip 20 | Remove-Item -Force -ErrorAction SilentlyContinue
} catch {}

[void]$form.ShowDialog()
