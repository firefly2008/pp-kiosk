# PP Kiosk

Kiosk voor een Windows-pc met een groot touchscreen (32 inch, Full HD):
dagstructuur-agenda, presentaties (PowerPoint en foto's), snelkoppelingen naar
Nederlandse mediaproviders en kijken/luisteren (video, radio, podcasts).
Geen cliëntprofielen. Beheer via een eenvoudige beheerpagina in de browser.

Gebaseerd op [Kijkluister](https://github.com/firefly2008/kijkluister) (Qt 6.8, QML + C++).

## Stand van zaken

| Stap | Inhoud | Status |
|---|---|---|
| 1 | Kijkluister kopiëren en opschonen, naam PP Kiosk | klaar |
| 2 | Startscherm + dagstructuur-agenda + beheerpagina | klaar |
| 3 | Media: kijk & luister, presentaties, PowerPoint-upload, snelkoppelingen | klaar |
| 4 | Testen op het touchscreen + release | gepland |

## Bouwen

Het makkelijkst in de cloud: elke push bouwt via `.github/workflows/windows-installer.yml`
(Actions → laatste run → Artifacts → `PPKiosk-installer`). Een push naar
`release/X.Y.Z` maakt ook een GitHub-release.

Lokaal (Visual Studio 2022 C++, CMake, Ninja, Qt 6.8+ msvc2022_64, Inno Setup 6),
vanuit een "x64 Native Tools Command Prompt":

```powershell
powershell -File scripts\build-installer.ps1 -QtDir C:\Qt\6.8.3\msvc2022_64 -Version 0.1.0
```

## Gegevens

Alles staat in `%LOCALAPPDATA%\PPKiosk`: `settings.ini`, `ppkiosk.log` en de cache.

## Beheerpagina

- Op de kiosk-pc zelf: open de browser en ga naar `http://localhost:8642` (geen code nodig).
- Vanaf een andere pc, tablet of telefoon op hetzelfde netwerk: houd op het kioskscherm de klok
  1,5 seconde vast. Dan staan het adres, de code en een QR-code in beeld.
- De dagplanning staat in `%LOCALAPPDATA%\PPKiosk\agenda.json`.
- Tabblad **Presentaties**: PowerPoint (.pptx/.ppt/.ppsx/.odp), PDF of foto's uploaden (max. 400 MB per bestand).
  PowerPoint wordt op de kiosk-pc omgezet met PowerPoint zelf; zonder PowerPoint met LibreOffice.
  Lukt geen van beide, upload dan een PDF. Het origineel blijft bewaard in `presentaties\<naam>\origineel`
  en is te downloaden, aan te passen en opnieuw te uploaden onder dezelfde naam.
- Tabblad **Snelkoppelingen**: tegels naar websites (NPO Start, NOS, Videoland, ...), opgeslagen in
  `snelkoppelingen.json`.
