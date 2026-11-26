# PP Kiosk: kiosk voor een groot touchscreen (dagstructuur, presentaties, media).
# Gebaseerd op Kijkluister (Windows-port van KDE Plasma Bigscreen).
#
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.24)
project(ppkiosk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 CONFIG REQUIRED COMPONENTS Core Gui Quick Qml Network Multimedia Concurrent Pdf TextToSpeech)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(ppkiosk WIN32
    src/main.cpp
    src/appmodel.cpp
    src/appmodel.h
    src/appiconprovider.cpp
    src/appiconprovider.h
    src/radiomodel.cpp
    src/radiomodel.h
    src/weather.cpp
    src/weather.h
    src/updater.cpp
    src/updater.h
    src/homebridge.cpp
    src/homebridge.h
    src/systemtools.cpp
    src/systemtools.h
    src/securestore.cpp
    src/securestore.h
    src/agenda.cpp
    src/agenda.h
    src/adminserver.cpp
    src/adminserver.h
    src/presentations.cpp
    src/presentations.h
    src/shortcuts.cpp
    src/speaker.cpp
    src/speaker.h
    src/activity.cpp
    src/menuboard.cpp
    src/activity.h
    src/menuboard.h
    src/stats.cpp
    src/stats.h
    src/backup.cpp
    src/backup.h
    src/listener.cpp
    src/listener.h
    src/news.cpp
    src/news.h
    src/shortcuts.h
    src/thirdparty/qrcodegen.cpp
    src/thirdparty/qrcodegen.hpp
    src/paths.h
)

set(PPKIOSK_VERSION "0.0.0" CACHE STRING "Versie van PP Kiosk (voor bijwerken)")
target_compile_definitions(ppkiosk PRIVATE PPKIOSK_VERSION="${PPKIOSK_VERSION}")

qt_add_resources(ppkiosk "icons" PREFIX "/" FILES assets/ppkiosk-256.png assets/web-192.png assets/web-512.png assets/web-maskable-512.png assets/logos/nlziet.png assets/logos/simone-fm.png assets/chime.wav)
qt_add_resources(ppkiosk "releasenotes" PREFIX "/" FILES releasenotes.json)
qt_add_resources(ppkiosk "admin" PREFIX "/" FILES admin/index.html)
qt_add_resources(ppkiosk "demo" PREFIX "/" FILES demo/diner-van-de-dag.jpg)
# Foto's voor het menubord (gemaakt met Figma AI)
qt_add_resources(ppkiosk "food" PREFIX "/food" BASE assets/food FILES assets/food/aardappel-vlees.jpg assets/food/burger.jpg assets/food/friet.jpg assets/food/kip-rijst.jpg assets/food/nasi.jpg assets/food/pannenkoek.jpg assets/food/pasta.jpg assets/food/pizza.jpg assets/food/salade.jpg assets/food/soep.jpg assets/food/stamppot.jpg assets/food/vis.jpg)
qt_add_resources(ppkiosk "glass" PREFIX "/glass" BASE assets/glass FILES assets/glass/violet.png assets/glass/blauw.png assets/glass/turquoise.png assets/glass/roze.png)
qt_add_resources(ppkiosk "lineicons" PREFIX "/icons" BASE assets/icons FILES assets/icons/presentaties.png assets/icons/stream.png assets/icons/snelkoppelingen.png assets/icons/spelletjes.png assets/icons/muziek.png assets/icons/week.png)
# PowerPoint omzetten (Windows): komt in de resources als :/scripts/ppt-export.ps1
set_source_files_properties(src/ppt-export.ps1 PROPERTIES QT_RESOURCE_ALIAS ppt-export.ps1)
set_source_files_properties(src/listen.ps1 PROPERTIES QT_RESOURCE_ALIAS listen.ps1)
qt_add_resources(ppkiosk "scripts" PREFIX "/scripts" FILES src/ppt-export.ps1 src/listen.ps1)

if(WIN32)
    target_sources(ppkiosk PRIVATE
        src/app.rc
        src/win32inputsystem.cpp
        src/win32inputsystem.h
    )
    target_compile_definitions(ppkiosk PRIVATE BIGSCREEN_HAVE_WIN32_INPUT)
else()
    target_sources(ppkiosk PRIVATE
        src/noopinputsystem.cpp
        src/noopinputsystem.h
    )
endif()

# Zonder alias komt het bestand in de resources onder .../qml/Main.qml terecht
set(PPKIOSK_QML_FILES qml/Main.qml qml/StreamScreen.qml qml/PresentationScreen.qml qml/ShortcutsScreen.qml qml/FocusButton.qml qml/OnScreenKeyboard.qml qml/GamesScreen.qml qml/WeekScreen.qml)
foreach(qml_file IN LISTS PPKIOSK_QML_FILES)
    get_filename_component(qml_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()

qt_add_qml_module(ppkiosk
    URI nl.ppkiosk
    VERSION 1.0
    QML_FILES
        ${PPKIOSK_QML_FILES}
)

target_link_libraries(ppkiosk PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::Network
    Qt6::Multimedia
    Qt6::Concurrent
    Qt6::Pdf
    Qt6::TextToSpeech
)

if(WIN32)
    target_link_libraries(ppkiosk PRIVATE user32 shell32 ole32 comctl32 ws2_32 iphlpapi crypt32)
endif()

# Installatie: alles plat in een map, zodat windeployqt + Inno Setup er een installer van maken
set_target_properties(ppkiosk PROPERTIES OUTPUT_NAME PPKiosk)
install(TARGETS ppkiosk RUNTIME DESTINATION .)
