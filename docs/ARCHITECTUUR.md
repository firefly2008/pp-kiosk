# Architectuur van Kijkluister

Windows-app in Qt 6.8 (Qt Quick/QML + C++), SDL3 voor gamepads, Inno Setup voor de installer.

## Opbouw
- `src/main.cpp`: start de app, maakt alle modellen aan en geeft ze als context-eigenschap aan QML (`appModel`, `iptv`, `recorder`, `library`, `parental`, `apiKeys`, `streamRelay`, `homeBridge`, ...).
- `qml/Main.qml`: startpagina (klassiek en tabbladen-layout), alle menu's (`openSettings`, `handleMenuAction`), sleutel- en profielschermen, en de zelftests (`selfTest`).
- Schermen: `IptvScreen` (live tv, speler, fout/retry), `GuideGrid`/`EpgPanel` (gids), `RecordPanel`, `LibraryScreen`/`LibrarySetup` (films en series), `RadioPlayer`, `FrameScreen` (schilderij), `KeysOverlay` (eigen sleutels), `MenuOverlay` (alle keuzemenu's).

## C++-modules (src/)
| Module | Taak |
|---|---|
| appmodel, radiomodel, resumemodel, usagemodel, nowtvmodel | tegels en rijen op de startpagina |
| iptvmodel | zenderlijsten (m3u, Xtream), gids (XMLTV), herinneringen, series |
| recorder, streamrelay | opnemen met ffmpeg; kijken via lokale doorgifte (VLC-User-Agent) |
| medialibrary, mpcplayer | films en series (TMDB/TVmaze), afspelen in MPC-HC |
| components | extra onderdelen ophalen (ffmpeg, libCEC) en gedeelde hulpjes `ffmpegPath()` / `freeUdpPort()` |
| apikeys, securestore | eigen sleutels; versleuteling met Windows DPAPI |
| parental, profiles | kinderslot (pincode, vergrendelde tegels, kijktijd) en profielen |
| homebridge, win32inputsystem, gamepadnavigator, cec | Home/Esc, toetsenbord en gamepad, tv-afstandsbediening |
| remoteserver | telefoon als afstandsbediening (http op het thuisnetwerk) |
| updater, systemtools, weather, artframe, photosearch | bijwerken, Windows-functies, weer, schilderij, foto's |

## Gegevens
Alles in `%LOCALAPPDATA%\Kijkluister`: `settings.ini` (gevoelige waarden als `_dpapi`, versleuteld voor dit Windows-account), `kijkluister.log`, `images\`, opnames in de gekozen map.
Taal: Nederlands is de bron (`qsTr`); Engels via `i18n/kijkluister_en.ts/.qm` en `i18n/tools/en_dict.py`.

## Release en test
1. Commit op `main`, daarna `git push origin main:refs/heads/release/X.Y.Z`; het versienummer komt uit de branchnaam.
2. `.github/workflows/windows-installer.yml` bouwt, draait de rooktest (elke `BIGSCREEN_SELFTEST`-modus start de app en zoekt een logregel uit `$checks`) en publiceert pas als alles groen is.
3. Testsleutels (alleen voor de rooktest): `BIGSCREEN_TEST_UNSPLASH`, `BIGSCREEN_TEST_TMDB` uit GitHub-geheimen. Het programma zelf bevat geen sleutels.
4. Releasenotes: `releasenotes.json` (nieuwste bovenaan, `notes` en `notes_en`).

## Bekende ruwe randen (opruimlijst)
- `Main.qml` (ca. 2900 regels) doet te veel: menu's, tabbladen en zelftests horen in eigen bestanden.
- `handleMenuAction` is een lange if/else-keten; beter een tabel actie → functie.
- `iptvmodel.cpp` (ca. 1450 regels) kan splitsen in bronnen, gids en herinneringen.
- Zelftestcode staat tussen de gewone code (`Main.qml`, `IptvScreen.qml`).
Dit gaat stap voor stap, met na elke stap alle rooktests.
