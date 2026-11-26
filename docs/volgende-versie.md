# Plan volgende versie

Gebouwd in 4.10.0: punt 1 (eigen sleutels), 2 (kinderslot versleuteld + instellingen achter pincode), 3 (menu in zes groepen), 4 (Esc instelbaar). Punt 5 (code opschonen) gedeeltelijk in 4.11.0: gedeelde ffmpeg/poort-hulpjes en docs/ARCHITECTUUR.md. Nog open: Main.qml en iptvmodel.cpp opsplitsen, handleMenuAction als tabel, zelftests uit de productiecode (zie punt 5 hieronder).

## (4.10.0, gebouwd) oorspronkelijk plan

Gevraagd op 21-11-2026, nog niet gebouwd ("niet nu").

## 1. Eigen TMDB- en Unsplash-sleutels (ingebouwde sleutels eruit)
- Ingebouwde sleutels (XOR-verhuld) verdwijnen uit het programma en uit de build (geen TMDB_TOKEN/UNSPLASH_TOKEN-secrets meer nodig).
- Gebruiker vult zelf zijn sleutel in: Instellingen → Sleutels (invoer via schermtoetsenbord of telefoonpagina). Opslag versleuteld via securestore (DPAPI), nooit in logbestand.
- Op dat scherm per dienst een uitleg en het adres waar de sleutel wordt aangemaakt:
  - TMDB: https://www.themoviedb.org/settings/api (gratis account, "API Read Access Token")
  - Unsplash: https://unsplash.com/oauth/applications (nieuwe app, "Access Key")
  Adres als tekst en als QR-code, zodat het met de telefoon is te openen.
- Zonder sleutel: functie rustig uitgeschakeld met een melding die naar dit scherm verwijst (geen foutmelding). Eerste-start-hulp (Welkom) noemt het kort.
- Bestaande installaties: sleutel ontbreekt na de update; eenmalige melding. Releasenotes vermelden dit duidelijk.
- Controleknop "Testen": één verzoek naar de dienst, toont "Sleutel werkt" of "Sleutel geweigerd".

## 2. Kinderslot en kijktijd beter beschermen (voorstel, nog te bevestigen)
- Pincode, kinderslot en kijktijdlimieten uit settings.ini halen en opslaan in securestore (DPAPI, gebonden aan het Windows-account van de ouder), met een handtekening zodat aanpassen van het bestand wordt opgemerkt (dan: slot blijft aan / standaard streng).
- Pincode alleen als hash met salt, nooit leesbaar.
- Eerlijke beperking in de uitleg: een kind met een eigen Windows-beheerdersaccount kan dit nooit helemaal tegenhouden. Advies in de app: kinderen een gewoon account geven.
- Vraag aan gebruiker vóór bouwen: bedoel je alleen dit, of ook een aparte "ouderkluis" met aparte pincode voor instellingen?

## Beveiliging: nog open
- (Wordt in de volgende versie aangepakt, zie punt 1.) De ingebouwde TMDB- en Unsplash-sleutels zijn alleen verhuld.
- (Zie punt 2.) Kinderslot en kijktijd staan in settings.ini.
- Telefoonpagina gaat via http op het thuisnetwerk (geen https): iemand op hetzelfde wifi kan in theorie de code onderscheppen. Echte oplossing: de eigen Android-app met gekoppelde sleutel.


## 3. Instellingenmenu opnieuw indelen (voorstel, 21-11-2026, nog niet gebouwd)

### Wat nu niet klopt (consumentenblik)
- Twee lagen met een vage scheiding: "Instellingen…" (8 items) en daarin "Meer instellingen…" (17 items, plat, door elkaar). Een gebruiker weet niet waar iets staat.
- "Meer instellingen" is een rommella: aan/uit-schakelaars, submenu's en acties (back-up, logbestand, update) staan zonder groepering door elkaar.
- Dingen die bij elkaar horen staan los: Kinderslot, Kijktijd, Profielen en "Bij opstarten vragen wie er kijkt" (verspreid over 4 plekken, kijktijd staat zelfs ver van het kinderslot).
- Weergave staat op twee plekken: Achtergrond, Layout, Tekstgrootte, Grote naam in het eerste menu; Nachtmodus, Sfeerstand, Taal in het tweede.
- "Tegel toevoegen" staat vooraan, maar "Verwijderde tegels terugzetten" onderaan het menu; "Diensten kiezen…" is een onduidelijke naam (het is eigenlijk de welkomsthulp) en staat tussen weergave-opties.
- "Opnames opruimen" hoort bij Live tv/Opnames, niet bij algemene instellingen. "Weer" is een startpagina-onderdeel.
- Technische dingen (Logbestand delen, Release-info, Zoeken naar versie, CEC, Windows-instellingen) staan op gelijke hoogte met dagelijkse keuzes.
- Inconsistent: "Sluiten" én "Annuleren"; toestand soms in de naam met dubbele punt ("Nachtmodus: uit"), soms tussen haakjes ("Kinderslot… (uit)"); "Taal / Language" is tweetalig maar de rest niet; "Layout: tabbladen (kies om terug te gaan naar klassiek)" is een zin, geen instelling.
- Slaaptimer zit naast Instellingen, niet erin (prima als snelkoppeling, maar niet vindbaar als je in Instellingen zoekt).
- Hulp (sneltoetsen/handleiding) en Release-info staan niet bij elkaar.

### Voorstel: één menu met 6 groepen (max 5-7 items per groep)
1. **Startpagina**: Tegel toevoegen · Verwijderde tegels terugzetten · Rijen (verplaatsen/inklappen: "Weinig gebruikte rijen inklappen") · Weer · Radio hervatten bij opstarten · Welkomsthulp opnieuw ("Diensten kiezen" hernoemen naar "Welkomsthulp: diensten en apps kiezen")
2. **Uiterlijk**: Layout (Klassiek / Tabbladen, als keuzelijst met vinkje) · Achtergrond · Tekstgrootte · Grote naam en logo · Nachtmodus · Sfeerstand (Schilderij) · Taal
3. **Gezin en veiligheid**: Profielen · Wie kijkt er? (vragen bij opstarten) · Kinderslot (aan/uit, pincode) · Kijktijd per dag · (later) ouderkluis voor instellingen
4. **Geluid en apparaten**: Geluid via… · Tv-afstandsbediening (HDMI-CEC) · Telefoon als afstandsbediening · Windows: wifi, bluetooth, beeld
5. **Live tv en opnames**: Opnames opruimen · bronnen/aanbieder · (later) sleutels TMDB/Unsplash, zie punt 1
6. **Onderhoud en hulp**: Zoeken naar een nieuwe versie · Release-info · Back-up maken/terugzetten · Logbestand delen (uitleg: "voor hulp aan de maker, kan gegevens bevatten") · Sneltoetsen/handleiding · Starten met Windows

### Regels voor alle menu's
- Altijd dezelfde stijl: "Naam" links, huidige waarde rechts ("Nachtmodus   Uit"), Enter opent of wisselt. Geen "aan/uit" in de naam.
- Keuzes (Layout, Taal, Nachtmodus) openen een keuzelijst met vinkje, niet een zin in de menutekst.
- Eén woord voor sluiten: "Terug" in submenu's, Esc/B ook. "Annuleren" alleen bij bevestigingen.
- Bovenaan het menu de meest gebruikte dingen (Weergave, Tegel toevoegen), onderaan Onderhoud en hulp.
- Elke groep een korte uitleg onder de titel (één regel) waar een keuze niet vanzelf spreekt (Sfeerstand, Kijktijd, Doorgifte).
- Ouderkluis: Gezin en veiligheid vraagt de pincode als het Kinderslot aanstaat (nu kan een kind via Instellingen bij de limieten).
- Zoekveld in Instellingen ("typ om te zoeken") zodra het menu groter wordt dan ~25 items.

### Waar op te letten bij bouwen
- Alle handleActions blijven bestaan (alleen de indeling verandert), dus de bestaande tests blijven gelden; testmodes 'settingsmenu', 'parental', 'kidslimit', 'cleanup', 'audio', 'backup', 'profiles' controleren op nieuwe menu-opbouw.
- Alle nieuwe teksten vertalen (en_dict.py) en releasenotes vermelden dat instellingen verhuisd zijn, met een klein "Waar is X gebleven?" in de help.


## 4. Escape-toets: terug naar de startpagina vanuit alle apps (onderzocht 21-11-2026, nog niet gebouwd)

### Nu
- Alleen **Esc 1 seconde vasthouden** brengt je vanuit een externe app/website naar Kijkluister (HomeBridge, `src/homebridge.cpp`, timer 1000 ms, alleen als `m_external`). Een korte Esc gaat gewoon naar de app (sluit dialoogjes, verlaat volledig scherm).
- Home-toets van een afstandsbediening, Xbox-knop en CEC-home doen het direct. Gamepad "Terug" 900 ms vasthouden ook.
- Wat `goHome` doet: Edge, Chrome en MPC-HC sluiten (WM_CLOSE), elk ander programma wordt geminimaliseerd, Kijkluister komt naar voren. Zelf geopende browservensters worden altijd gesloten.

### Risico's / te checken bij bouwen
- Een korte Esc is te dubbelzinnig: in Netflix/YouTube verlaat Esc het volledige scherm, in een spel opent het een menu. Per se direct naar huis zou dan elke keer de app afsluiten. Voorstel: dubbele Esc (twee keer snel) óf vasthouden inkorten naar ~0,6 s, en kort "Houd Esc vast om terug te gaan" tonen als de app opent (nu staat dat alleen in de Help).
- Voorstel: Esc-gedrag instelbaar (Instellingen → Gezin/Apparaten): "Vasthouden (standaard)", "Dubbel indrukken", "Altijd direct".
- Programma's (geen browser) worden geminimaliseerd, niet afgesloten: ze blijven op de achtergrond draaien (geluid, spel). Beslissen: voor bekende speler-apps (VLC, Kodi, Spotify) afsluiten of pauzeren, voor de rest minimaliseren + melding.
- Testen op: Edge-app (Netflix/Disney+), Chrome, MPC-HC, zelf toegevoegd .exe, volledig-scherm-programma (spel), een programma dat Esc afvangt of als beheerder draait (de toetshook ziet dan geen toets: bij "als administrator" draaiende apps werkt WH_KEYBOARD_LL niet; dan blijft Home/Xbox-knop de uitweg, vermelden in Help).
- Smoke-test toevoegen: simuleer Esc-vasthouden met een extern venster (bijv. Kladblok) en controleer "Home: Esc held" + dat Kijkluister weer vooraan staat.

## 5. Code opschonen en structureren (onderzocht 21-11-2026, nog niet gebouwd)

### Bevindingen (ca. 21.800 regels)
- `qml/Main.qml` is 2818 regels en doet alles: startpagina, tabbladen-layout, alle menu's, `handleMenuAction` (grote if/else-keten), selftests, toasts. Belangrijkste knelpunt.
- `src/iptvmodel.cpp` 1459 regels (kanalen, bronnen, Xtream, EPG, herinneringen, series), `qml/IptvScreen.qml` 1119 (speler, fout/retry, opname- en gidspanelen, test-timers), `src/medialibrary.cpp` 860, `qml/LibraryScreen.qml` 771.
- Testcode (selftest-takken, `tabsTest`, guideTestTimer, recordTestTimer) staat tussen de productiecode in Main.qml en IptvScreen.qml.
- Vaste teksten met hardcoded indeling van menu's in Main.qml (zie punt 3).

### Voorstel (zonder gedrag te veranderen)
1. Main.qml opsplitsen: `SettingsMenus.qml` (alle menu-opbouw + handleMenuAction in een tabel actie→functie), `HomeTabs.qml` (tabbladen-layout), `SelfTests.qml` (alle testcode, alleen geladen bij BIGSCREEN_SELFTEST). Main.qml blijft alleen de bedrading.
2. `handleMenuAction` vervangen door een kaart van actie → handler, en acties met parameter ("audio-set:ID") via één prefix-parser. Elke menu-actie op één plek.
3. IptvScreen.qml: afspeelstatus (poging, retry, doorgifte, fout) naar een eigen `PlaybackState.qml`/C++-klasse; testtimers naar SelfTests.
4. `iptvmodel.cpp` splitsen: `iptvsources` (m3u/Xtream), `iptvepg` (XMLTV), `iptvreminders`. Eén ding per bestand.
5. Dubbele ffmpeg-zoekfunctie (recorder.cpp en streamrelay.cpp) naar `components` (één `ffmpegPath()`), idem `freeUdpPort`.
6. Hardcoded paden, tijden en teksten naar constanten (`paths.h` bestaat al), geen magische getallen in de QML (unit-factoren).
7. Een korte `docs/ARCHITECTUUR.md`: welke module doet wat, hoe een release/test werkt.

### Veilig bouwen
- Eerst alleen verplaatsen, geen gedrag wijzigen; na elke stap volledige smoke-tests (alle modes uit de workflow moeten groen blijven).
- Eigen versie hiervoor (bijv. 4.11.0, "onder de motorkap") zodat een fout makkelijk terug te draaien is en niet vermengd raakt met nieuwe functies.
