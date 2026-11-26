# Vertaling (Engels)

Nieuwe of gewijzigde teksten:
1. In QML `qsTr("…")`, in C++ `tr("…")`.
2. `lupdate src/*.cpp src/*.h qml/*.qml -ts i18n/kijkluister_en.ts -no-obsolete`
3. Engelse tekst toevoegen in `i18n/tools/en_dict.py`, dan `python3 i18n/tools/fill.py` (vult alles in, plus radio-slogans en groepsnamen).
4. `lrelease i18n/kijkluister_en.ts -qm i18n/kijkluister_en.qm` en het .qm-bestand meecommitten.
