# Kijkluister -- Windows PoC skeleton (Fase 0/1)

Startpunt voor **Optie A** uit het migratieplan ("Kijkluister -> Windows:
Architectuur-audit & Migratieplan"): een losse, standalone Qt Quick kiosk-app
die de rol van KWin/ksmserver/envmanager/inputhandler-portal vervangt, zonder
D-Bus en zonder Plasma-sessie.

Dit skeleton is **niet gebouwd of getest** (geen Windows/Qt-toolchain in deze
omgeving) -- het is het vertrekpunt voor Fase 0/1 van het stappenplan, niet
een werkende build. Zie het rapport voor de volledige architectuur-audit,
code-toelichting en de rest van het stappenplan.

## Build (Windows, MSVC + vcpkg)

```powershell
vcpkg install --triplet x64-windows
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
build\Release\bigscreen-host.exe --screen=0
```

## Wat hier wel in staat

- `CMakeLists.txt` -- WIN32-conditionele dependencies, vcpkg-manifest-ready
- `src/main.cpp` -- fullscreen QQuickView op een gekozen monitor, SDL3-gamepad-init,
  screenshot-helper (vervangt de KWin D-Bus ScreenShot2-aanroep)
- `src/inputsystem.h` + `src/win32inputsystem.*` -- vervangt
  `inputhandler/xdgremotedesktopsystem.*` (XDG-portal) door directe Win32
  `SendInput()`-aanroepen
- `qml/Main.qml` -- placeholder; hier hangt later de echte shell-UI
  (`containments/homescreen`) aan

## Wat hier bewust niet in staat

Alles wat in het migratieplan onder "Architectuur-audit" als te vervangen of te
laten vervallen is gemarkeerd: BluezQt/wifi/display-KCM's, D-Bus-services
(envmanager/kded), libcec/CEC, de KF6-modules zonder Windows-build. Zie dat
rapport voor de volgorde waarin je dat oppakt (Fase 2-5).

## Installer maken

Twee routes, beide leveren `BigscreenHost-6.8.2-x64-setup.exe`:

1. **Cloud (makkelijkst):** zet deze map in een GitHub-repo. De workflow
   `.github/workflows/windows-installer.yml` bouwt alles op een Windows-runner;
   download de installer onder Actions -> laatste run -> Artifacts.
2. **Lokaal:** installeer Visual Studio 2022 (C++), CMake, Ninja, Qt 6.8+
   (msvc2022_64), vcpkg en Inno Setup 6, en draai vanuit een
   "x64 Native Tools Command Prompt":
   `powershell -File scripts\build-installer.ps1 -QtDir C:\Qt\6.8.3\msvc2022_64 -VcpkgRoot C:\vcpkg`

De installer biedt optioneel een bureaubladsnelkoppeling en autostart bij aanmelden.
Shell-vervanging (Winlogon) zit er bewust niet in: te riskant zolang de app een PoC is.
