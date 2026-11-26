# Kijkluister: Windows-port van Plasma Bigscreen (tot versie 3.7 "Bigscreen Host")
# Optie A (zie migratieplan): losse kiosk-app, geen KWin/Plasma-sessie.
#
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.24)
project(bigscreen-host LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 CONFIG REQUIRED COMPONENTS Core Gui Quick Qml Network Multimedia Concurrent)
find_package(ZLIB REQUIRED)
qt_standard_project_setup(REQUIRES 6.8)
find_package(SDL3 CONFIG REQUIRED)

qt_add_executable(bigscreen-host WIN32
    src/main.cpp
    src/appmodel.cpp
    src/appmodel.h
    src/appiconprovider.cpp
    src/appiconprovider.h
    src/gamepadnavigator.cpp
    src/gamepadnavigator.h
    src/medialibrary.cpp
    src/medialibrary.h
    src/apikeys.cpp
    src/apikeys.h
    src/radiomodel.cpp
    src/radiomodel.h
    src/resumemodel.cpp
    src/resumemodel.h
    src/components.cpp
    src/components.h
    src/mpcplayer.cpp
    src/mpcplayer.h
    src/recorder.cpp
    src/recorder.h
    src/streamrelay.cpp
    src/streamrelay.h
    src/artframe.cpp
    src/artframe.h
    src/iptvmodel.cpp
    src/iptvmodel.h
    src/parental.cpp
    src/parental.h
    src/weather.cpp
    src/weather.h
    src/updater.cpp
    src/updater.h
    src/cec.cpp
    src/cec.h
    src/homebridge.cpp
    src/homebridge.h
    src/photosearch.cpp
    src/systemtools.cpp
    src/securestore.cpp
    src/securestore.h
    src/remoteserver.cpp
    src/nowtvmodel.cpp
    src/nowtvmodel.h
    src/remoteserver.h
    src/thirdparty/qrcodegen.cpp
    src/thirdparty/qrcodegen.hpp
    src/systemtools.h
    src/profiles.cpp
    src/profiles.h
    src/usagemodel.cpp
    src/usagemodel.h
    src/photosearch.h
)

set(KIJKLUISTER_VERSION "0.0.0" CACHE STRING "Versie van Kijkluister (voor bijwerken)")
target_compile_definitions(bigscreen-host PRIVATE KIJKLUISTER_VERSION="${KIJKLUISTER_VERSION}")

# Pictogram (getekend door assets/make_icon.py)
qt_add_resources(bigscreen-host "icons" PREFIX "/" FILES assets/kijkluister-256.png assets/web-192.png assets/web-512.png assets/web-maskable-512.png assets/logos/nlziet.png assets/logos/simone-fm.png)
# Release-info (Aanpassen → Release-info); bij elke nieuwe versie bovenaan aanvullen
qt_add_resources(bigscreen-host "releasenotes" PREFIX "/" FILES releasenotes.json)
qt_add_resources(bigscreen-host "i18n" PREFIX "/" FILES i18n/kijkluister_en.qm)

if(WIN32)
    target_sources(bigscreen-host PRIVATE
        src/app.rc
        src/win32inputsystem.cpp
        src/win32inputsystem.h
    )
    target_compile_definitions(bigscreen-host PRIVATE BIGSCREEN_HAVE_WIN32_INPUT)
else()
    # Op Linux/macOS valt input-injectie voorlopig terug op een no-op stub,
    # zodat dit skeleton op elk platform blijft compileren tijdens de PoC-fase.
    target_sources(bigscreen-host PRIVATE
        src/noopinputsystem.cpp
        src/noopinputsystem.h
    )
endif()

# Zonder alias komt het bestand in de resources onder .../host/qml/Main.qml terecht
set(BIGSCREEN_QML_FILES qml/Main.qml qml/BannerTile.qml qml/ThumbTile.qml
    qml/KeysOverlay.qml qml/QuickTile.qml qml/MenuOverlay.qml qml/PlacesPicker.qml qml/SectionGrid.qml qml/AddTileOverlay.qml qml/LibraryScreen.qml qml/LibrarySetup.qml qml/FocusButton.qml qml/PosterTile.qml qml/IptvScreen.qml qml/IptvSources.qml qml/EpgPanel.qml qml/FrameScreen.qml qml/RecordPanel.qml qml/HintBar.qml qml/WelcomeOverlay.qml qml/RadioPlayer.qml qml/PinPad.qml qml/OnScreenKeyboard.qml qml/SearchPicker.qml qml/HomeButton.qml qml/ReleaseInfo.qml qml/ResumeScreen.qml qml/HelpOverlay.qml qml/GuideGrid.qml qml/RemoteOverlay.qml)
foreach(qml_file IN LISTS BIGSCREEN_QML_FILES)
    get_filename_component(qml_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()

qt_add_qml_module(bigscreen-host
    URI org.kde.plasma.bigscreen.host
    VERSION 1.0
    QML_FILES
        ${BIGSCREEN_QML_FILES}
)

target_link_libraries(bigscreen-host PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::Network
    Qt6::Multimedia
    Qt6::Concurrent
    ZLIB::ZLIB
    SDL3::SDL3
)

if(WIN32)
    target_link_libraries(bigscreen-host PRIVATE user32 shell32 ole32 comctl32 ws2_32 iphlpapi crypt32)
endif()

# Installatie: alles plat in een map, zodat windeployqt + Inno Setup er een installer van maken
set_target_properties(bigscreen-host PROPERTIES OUTPUT_NAME Kijkluister)
install(TARGETS bigscreen-host RUNTIME DESTINATION .)
if(WIN32)
    install(FILES $<TARGET_RUNTIME_DLLS:bigscreen-host> DESTINATION .)
    # zlib (vcpkg) is een "UNKNOWN" importbibliotheek en komt niet vanzelf mee: DLL zelf kopiëren
    get_filename_component(ZLIB_BIN_DIR "${ZLIB_INCLUDE_DIRS}/../bin" ABSOLUTE)
    # (de naam wisselt per vcpkg-versie: z.dll, zlib1.dll of libz.dll)
    file(GLOB ZLIB_DLLS "${ZLIB_BIN_DIR}/z.dll" "${ZLIB_BIN_DIR}/zlib*.dll" "${ZLIB_BIN_DIR}/libz*.dll")
    if(NOT ZLIB_DLLS)
        message(FATAL_ERROR "Geen zlib-DLL gevonden in ${ZLIB_BIN_DIR}")
    endif()
    message(STATUS "zlib DLL's: ${ZLIB_DLLS}")
    install(FILES ${ZLIB_DLLS} DESTINATION .)
endif()

# --- Linux-only dependencies uit het origineel (plasma-bigscreen 6.8.2) ---
# Bewust NIET meegenomen in dit Windows-skeleton, zie het migratieplan (Architectuur-audit):
#   KF6 (BluezQt, DBusAddons, GlobalAccel, WindowSystem, KIO, ...), Plasma/PlasmaActivities,
#   LibKWorkspace, Qt6::WaylandClient, PlasmaWaylandProtocols, Wayland (pkg-config), systemd,
#   libcec (optioneel terug toe te voegen), QCoro6, Qt6::DBus, Qt6::WebEngineCore/Quick,
#   Qt6::Multimedia. Die laatste drie zijn wel kandidaten om in een volgende fase toe te voegen
#   (webapp-viewer, uvcviewer, audio-device-chooser) -- zie het migratieplan voor de volgorde.
