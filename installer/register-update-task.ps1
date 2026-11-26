# Door de installer uitgevoerd (met beheerdersrechten): maakt de geplande taak
# "PP Kiosk bijwerken" die ppkiosk-update.ps1 met hoogste rechten draait.
# PP Kiosk zelf (gewone rechten) mag de taak alleen starten, niet wijzigen.
# SPDX-License-Identifier: GPL-2.0-or-later
param([string]$AppDir, [switch]$Remove)
$name = 'PP Kiosk bijwerken'
if ($Remove) {
    Unregister-ScheduledTask -TaskName $name -Confirm:$false -ErrorAction SilentlyContinue
    exit 0
}
$script = Join-Path $AppDir 'ppkiosk-update.ps1'
$user = [Security.Principal.WindowsIdentity]::GetCurrent()
$action = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$script`""
$principal = New-ScheduledTaskPrincipal -UserId $user.Name -LogonType Interactive -RunLevel Highest
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit (New-TimeSpan -Hours 13) -MultipleInstances IgnoreNew
Register-ScheduledTask -TaskName $name -Action $action -Principal $principal -Settings $settings -Force | Out-Null
# Rechten op de taak: beheerders en systeem alles, deze gebruiker alleen lezen en starten
$sid = $user.User.Value
$service = New-Object -ComObject 'Schedule.Service'
$service.Connect()
$task = $service.GetFolder('\').GetTask($name)
$task.SetSecurityDescriptor("D:(A;;FA;;;BA)(A;;FA;;;SY)(A;;GRGX;;;$sid)", 0)
"Taak '$name' gemaakt voor $($user.Name)"
