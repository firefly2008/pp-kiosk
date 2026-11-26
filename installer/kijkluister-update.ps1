# Kijkluister automatisch bijwerken (draait via de geplande taak "Kijkluister bijwerken",
# met hoogste rechten, zodat er geen Windows-melding (UAC) komt).
# 1. Haalt de nieuwste installer op uit de openbare repo firefly2008/kijkluister-releases
#    (vaste bron; niets van buitenaf bepaalt wat hier wordt uitgevoerd).
# 2. Meldt Kijkluister "ready" (update-status.txt in %LOCALAPPDATA%\Kijkluister).
# 3. Wacht tot Kijkluister zegt "nu" (update-now) en afgesloten is, installeert stil
#    en start Kijkluister weer als gewone gebruiker.
# SPDX-License-Identifier: GPL-2.0-or-later
$ErrorActionPreference = 'Stop'
$app = Split-Path -Parent $MyInvocation.MyCommand.Path
$work = Join-Path $app 'update'
$data = Join-Path $env:LOCALAPPDATA 'Kijkluister'
$status = Join-Path $data 'update-status.txt'
$now = Join-Path $data 'update-now'
$log = Join-Path $work 'update.log'
New-Item -ItemType Directory -Force $work | Out-Null
# Downloadmap alleen schrijfbaar voor beheerders en SYSTEM (de installer wordt met hoogste rechten gestart)
try {
    $acl = New-Object System.Security.AccessControl.DirectorySecurity
    $acl.SetAccessRuleProtection($true, $false)
    foreach ($sid in 'S-1-5-18', 'S-1-5-32-544') {
        $id = New-Object System.Security.Principal.SecurityIdentifier $sid
        $acl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule $id, 'FullControl', 'ContainerInherit,ObjectInherit', 'None', 'Allow'))
    }
    $users = New-Object System.Security.Principal.SecurityIdentifier 'S-1-5-32-545'
    $acl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule $users, 'ReadAndExecute', 'ContainerInherit,ObjectInherit', 'None', 'Allow'))
    Set-Acl -Path $work -AclObject $acl
} catch { }
New-Item -ItemType Directory -Force $data | Out-Null
function Log($text) { "$(Get-Date -Format s) $text" | Add-Content $log }
function Status($text) { Set-Content -Path $status -Value $text -Encoding UTF8 }

try {
    Remove-Item $now -ErrorAction SilentlyContinue
    $key = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{6B0D5C1E-8E0B-4D53-9A61-B16C5C3EE2A7}_is1'
    $installed = (Get-ItemProperty $key -ErrorAction SilentlyContinue).DisplayVersion
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $release = Invoke-RestMethod -Uri 'https://api.github.com/repos/firefly2008/kijkluister-releases/releases/latest' `
        -Headers @{ 'User-Agent' = 'Kijkluister-updater' } -TimeoutSec 60
    $latest = $release.tag_name.TrimStart('v')
    Log "Installed $installed, latest $latest"
    if ($installed -and ([version]$latest -le [version]$installed)) {
        Status "current $installed"
        exit 0
    }
    $asset = $release.assets | Where-Object { $_.name -like '*-x64-setup.exe' } | Select-Object -First 1
    if (-not $asset) { throw 'Geen installer in de release' }
    $setup = Join-Path $work 'Kijkluister-setup.exe'
    Status "downloading $latest"
    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $setup -UseBasicParsing -TimeoutSec 1800
    if ((Get-Item $setup).Length -lt 1MB) { throw 'Download te klein' }
    # Controle met de SHA-256 die GitHub bij de release bewaart: verplicht (zonder controlegetal niet installeren)
    if (-not ($asset.digest -and $asset.digest.StartsWith('sha256:'))) { Remove-Item $setup -Force; throw 'Geen controlegetal bij de release' }
    $expected = $asset.digest.Substring(7).ToLower()
    if ((Get-FileHash $setup -Algorithm SHA256).Hash.ToLower() -ne $expected) { Remove-Item $setup -Force; throw 'Controlegetal klopt niet' }
    Log "Downloaded $latest"
    Status "ready $latest"

    # Wachten (maximaal 12 uur) tot Kijkluister zegt dat er niets speelt en afsluit
    $deadline = (Get-Date).AddHours(12)
    while (-not (Test-Path $now)) {
        if ((Get-Date) -gt $deadline) { Log 'Timeout, later opnieuw'; exit 0 }
        Start-Sleep -Seconds 5
    }
    Status "installing $latest"
    for ($i = 0; $i -lt 30 -and (Get-Process Kijkluister -ErrorAction SilentlyContinue); $i++) { Start-Sleep -Seconds 1 }
    Get-Process Kijkluister -ErrorAction SilentlyContinue | Stop-Process -Force
    # Vlak voor het installeren nog een keer controleren (het bestand heeft uren kunnen wachten)
    if ((Get-FileHash $setup -Algorithm SHA256).Hash.ToLower() -ne $expected) { Remove-Item $setup -Force; throw 'Installer gewijzigd na download' }
    Log "Installing $latest"
    $p = Start-Process -FilePath $setup -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/CLOSEAPPLICATIONS' -Wait -PassThru
    Log "Installer exit $($p.ExitCode)"
    Remove-Item $now -ErrorAction SilentlyContinue
    Remove-Item $setup -ErrorAction SilentlyContinue
    Status "installed $latest"
    # Weer starten als gewone gebruiker (via Verkenner, niet met hoogste rechten)
    Start-Process explorer.exe -ArgumentList "`"$(Join-Path $app 'Kijkluister.exe')`""
} catch {
    Log "Fout: $_"
    Status "error $_"
}
