# Bouwt bigscreen-host en maakt de Windows-installer.
# Nodig: Visual Studio 2022 (C++), CMake, Ninja, Qt 6.8+ (msvc2022_64), vcpkg, Inno Setup 6.
# Gebruik:  .\scripts\build-installer.ps1 -QtDir C:\Qt\6.8.3\msvc2022_64 -VcpkgRoot C:\vcpkg
param(
    [Parameter(Mandatory)] [string]$QtDir,
    [Parameter(Mandatory)] [string]$VcpkgRoot,
    [string]$Version = "6.8.2"
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

# 1. Configureren + bouwen (uitvoeren vanuit een "x64 Native Tools" prompt of na vcvars64.bat)
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$QtDir" `
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows "-DKIJKLUISTER_VERSION=$Version"
if ($LASTEXITCODE) { throw "cmake configure mislukt" }
cmake --build build
if ($LASTEXITCODE) { throw "build mislukt" }

# 2. Installeren naar dist\ (exe + SDL3.dll)
if (Test-Path dist) { Remove-Item dist -Recurse -Force }
cmake --install build --prefix dist
if ($LASTEXITCODE) { throw "install mislukt" }

# 3. Qt-runtime erbij (DLL's, QML-modules, plugins, VC-runtime)
& "$QtDir\bin\windeployqt.exe" --release --qmldir qml --compiler-runtime dist\Kijkluister.exe
if ($LASTEXITCODE) { throw "windeployqt mislukt" }

# MPC-HC en ffmpeg zitten NIET in de installer: Kijkluister haalt ze zelf op van GitHub
# (zie src/components.cpp). ffmpeg hieronder is alleen gereedschap voor de tests.
# 3b. ffmpeg (gyan.dev) als testgereedschap
$ffDir = Join-Path $root "build\ffmpeg"
if (-not (Test-Path "$ffDir\ffmpeg.exe")) {
    New-Item -ItemType Directory -Force $ffDir | Out-Null
    $zip = Join-Path $ffDir "ffmpeg.zip"
    # gyan.dev, en als die niet antwoordt de bouw van BtbN op GitHub
    try {
        Invoke-WebRequest -UseBasicParsing "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip" -OutFile $zip
    } catch {
        Write-Host "gyan.dev niet bereikbaar ($($_.Exception.Message)); ffmpeg van GitHub (BtbN)"
        Invoke-WebRequest -UseBasicParsing "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip" -OutFile $zip
    }
    Expand-Archive $zip -DestinationPath "$ffDir\unz" -Force
    $exe = Get-ChildItem "$ffDir\unz" -Recurse -Filter ffmpeg.exe | Select-Object -First 1
    Copy-Item $exe.FullName "$ffDir\ffmpeg.exe"
    $lic = Get-ChildItem $exe.Directory.Parent.FullName -Filter LICENSE* | Select-Object -First 1
    if ($lic) { Copy-Item $lic.FullName "$ffDir\LICENSE-ffmpeg.txt" }
    Remove-Item "$ffDir\unz" -Recurse -Force; Remove-Item $zip
}
& "$ffDir\ffmpeg.exe" -version | Select-Object -First 1

# 4. Installer
$iscc = (Get-Command iscc -ErrorAction SilentlyContinue).Source
if (-not $iscc) { $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" }
& $iscc "/DAppVersion=$Version" installer\bigscreen-host.iss
if ($LASTEXITCODE) { throw "Inno Setup mislukt" }
Write-Host "Klaar: out\Kijkluister-$Version-x64-setup.exe"
