; Inno Setup 6 script -- PP Kiosk voor Windows 10/11 (x64)
; Eigen AppId: staat los van Kijkluister, beide kunnen naast elkaar geinstalleerd zijn
; Bouwen: iscc /DAppVersion=1.0.0 installer\ppkiosk.iss   (na scripts\build-installer.ps1 stap 1-3)
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#define AppName "PP Kiosk"
#define AppExe "PPKiosk.exe"

[Setup]
AppId={{BE578AD7-D41D-4D22-8451-E3E5C2B5BBFB}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=PP Kiosk (gebaseerd op Kijkluister / KDE Plasma Bigscreen)
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE-GPL-2.0.txt
OutputDir=..\out
OutputBaseFilename=PPKiosk-{#AppVersion}-x64-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
WizardStyle=modern
SetupIconFile=..\assets\ppkiosk.ico
UninstallDisplayIcon={app}\{#AppExe}

[Languages]
Name: "dutch"; MessagesFile: "compiler:Languages\Dutch.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Snelkoppeling op het bureaublad"; Flags: unchecked
Name: "autostart"; Description: "PP Kiosk automatisch starten bij aanmelden (kiosk-pc)"; Flags: unchecked

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion
; Automatisch bijwerken zonder Windows-melding (geplande taak met hoogste rechten)
Source: "ppkiosk-update.ps1"; DestDir: "{app}"; Flags: ignoreversion
Source: "register-update-task.ps1"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon
Name: "{userstartup}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: autostart

[Run]
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ""{app}\register-update-task.ps1"" -AppDir ""{app}"""; StatusMsg: "Automatisch bijwerken instellen..."; Flags: runhidden waituntilterminated
; Beheerpagina via het netwerk (werkt alleen met de code): binnenkomend toestaan zonder Windows-melding
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""PP Kiosk beheer"""; Flags: runhidden waituntilterminated
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall add rule name=""PP Kiosk beheer"" dir=in action=allow protocol=TCP localport=8642-8651 program=""{app}\{#AppExe}"" profile=any"; Flags: runhidden waituntilterminated
Filename: "{app}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Microsoft Visual C++ runtime installeren..."; Flags: waituntilterminated skipifdoesntexist
Filename: "{app}\{#AppExe}"; Description: "{#AppName} nu starten"; Flags: nowait postinstall skipifsilent unchecked
; Bijwerken vanuit PP Kiosk zelf: na de stille installatie meteen weer starten
Filename: "{app}\{#AppExe}"; Flags: nowait runasoriginaluser; Check: IsRelaunch

[UninstallRun]
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ""{app}\register-update-task.ps1"" -Remove"; Flags: runhidden waituntilterminated; RunOnceId: "PPKioskUpdateTask"
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""PP Kiosk beheer"""; Flags: runhidden waituntilterminated; RunOnceId: "PPKioskFirewall"
Filename: "{sys}\reg.exe"; Parameters: "delete HKCU\Software\Microsoft\Windows\CurrentVersion\Run /v ""PP Kiosk"" /f"; Flags: runhidden waituntilterminated; RunOnceId: "PPKioskAutostart"

[UninstallDelete]
Type: filesandordirs; Name: "{app}\update"

[Code]
function IsRelaunch: Boolean;
begin
  Result := ExpandConstant('{param:relaunch|0}') = '1';
end;
