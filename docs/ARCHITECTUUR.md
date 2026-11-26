# Architectuur van PP Kiosk

Windows-app in Qt 6.8 (Qt Quick/QML + C++), Inno Setup voor de installer.
Afgeleid van Kijkluister; alles voor live tv, opnemen, films/series, kinderslot,
profielen, gamepad en tv-afstandsbediening (CEC) is eruit gehaald.

## Opbouw
- `src/main.cpp`: start de app (fullscreen op het gekozen scherm, `--screen=N`), maakt de modellen aan en geeft ze aan QML.
- `qml/Main.qml`: startscherm: klok, mededelingen, nu/straks, de hele dag en drie tegels; lang op de klok drukken toont de beheergegevens.

## C++-modules (src/)
| Module | Taak |
|---|---|
| appmodel, appiconprovider | snelkoppelingen naar websites en programma's (tegels) |
| radiomodel | radiozenders |
| weather | weerbericht op het startscherm |
| updater | stil bijwerken via de geplande taak "PP Kiosk bijwerken" |
| homebridge, win32inputsystem | terug naar PP Kiosk vanuit een website of app (Home-knop, Esc) |
| agenda | dagstructuur: activiteiten per weekdag, nu/straks, mededelingen (`agenda.json`) |
| adminserver | beheerpagina (`admin/index.html`) op poort 8642; localhost zonder code, netwerk met code |
| systemtools, securestore | Windows-functies (autostart, back-up), versleuteling met DPAPI |

## Rooktest
`BIGSCREEN_SELFTEST=<modus>` start de app in testmodus; de workflow zoekt per modus een regel in het log
(`$checks` in `.github/workflows/windows-installer.yml`) en bewaart een schermafbeelding als artifact.
