# Bouwt PP Kiosk en maakt de Windows-installer.
# Nodig: Visual Studio 2022 (C++), CMake, Ninja, Qt 6.8+ (msvc2022_64), Inno Setup 6.
# Gebruik:  .\scripts\build-installer.ps1 -QtDir C:\Qt\6.8.3\msvc2022_64
param(
    [Parameter(Mandatory)] [string]$QtDir,
    [string]$Version = "0.0.0"
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

# 1. Configureren + bouwen (uitvoeren vanuit een "x64 Native Tools" prompt of na vcvars64.bat)
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtDir" "-DPPKIOSK_VERSION=$Version"
if ($LASTEXITCODE) { throw "cmake configure mislukt" }
cmake --build build
if ($LASTEXITCODE) { throw "build mislukt" }

# 2. Installeren naar dist\
if (Test-Path dist) { Remove-Item dist -Recurse -Force }
cmake --install build --prefix dist
if ($LASTEXITCODE) { throw "install mislukt" }

# 3. Qt-runtime erbij (DLL's, QML-modules, plugins, VC-runtime)
& "$QtDir\bin\windeployqt.exe" --release --qmldir qml --compiler-runtime dist\PPKiosk.exe
if ($LASTEXITCODE) { throw "windeployqt mislukt" }

# 4. Installer
$iscc = (Get-Command iscc -ErrorAction SilentlyContinue).Source
if (-not $iscc) { $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" }
& $iscc "/DAppVersion=$Version" installer\ppkiosk.iss
if ($LASTEXITCODE) { throw "Inno Setup mislukt" }
Write-Host "Klaar: out\PPKiosk-$Version-x64-setup.exe"
