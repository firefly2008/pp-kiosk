// Startpagina van Bigscreen voor Windows: klok, snelknoppen en rijen onder elkaar
// (Video, Muziek, Podcasts, Media) die meescrollen. Volledig te bedienen met
// pijltjestoetsen, Enter en een gamepad.
// Eigen QtQuick-versie van containments/homescreen (die aan Plasma vastzit).
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Dialogs
import QtQuick.Effects

Rectangle {
    id: root

    readonly property real unit: height / 100
    readonly property int columns: unit > 0 ? Math.max(4, Math.floor(width / (unit * 25))) : 5

    // Model en rij van de tegel waarvoor het aanpas-menu of de bestandskiezer open staat
    property var editModel: appModel
    property int editRow: -1

    color: "#0b0c0f"

    // Typen op het startscherm: meteen zoeken
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_HomePage) {
            goHomeScreen()
            event.accepted = true
            return
        }
        const overlayOpen = iptvScreen.visible || libraryScreen.visible || frameScreen.visible || radioPlayer.visible || menu.visible
                            || picker.visible || welcomeOverlay.visible || addOverlay.visible || pinPad.visible || placesPicker.visible
                            || resumeScreen.visible || releaseInfo.visible || helpOverlay.visible || remoteOverlay.visible || kidsLock.visible
        // F3 (of Zoeken op de telefoon): zoekscherm
        if (!overlayOpen && (event.key === Qt.Key_F3 || event.key === Qt.Key_Search)) {
            root.openPicker("all", qsTr("Zoeken"), root.currentSection ? root.currentSection.grid : quickRow)
            event.accepted = true
            return
        }
        // Cijfers 1-9: tegel 1-9 van de bovenste rij openen
        if (!overlayOpen && event.text.length === 1 && /[1-9]/.test(event.text) && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier))) {
            const top = sectionDefs.length > 0 ? sectionDefs[0].model : null
            const n = parseInt(event.text) - 1
            if (top && n < top.count) {
                console.info("Number key", event.text, top.nameAt(n))
                launch(top, n)
            }
            event.accepted = true
            return
        }
        if (!overlayOpen && event.text.length === 1 && /[a-zA-Z0]/.test(event.text) && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier))) {
            root.openPicker("all", qsTr("Zoeken"), root.currentSection ? root.currentSection.grid : quickRow, event.text)
            event.accepted = true
        }
    }

    // De rijen op de startpagina, van boven naar beneden
    // Rijen in de volgorde die je zelf kiest (Menu op een tegel: Rij omhoog/omlaag).
    // Standaard staat Verder kijken onder Media.
    readonly property var allSectionDefs: [
        { title: "Vaak gebruikt", label: qsTr("Vaak gebruikt"), model: usage },
        { title: "Nu op tv", label: qsTr("Nu op tv"), model: nowTv },
        { title: "Video", label: qsTr("Video"), model: webVideo },
        { title: "Muziek", label: qsTr("Muziek"), model: webMusic },
        { title: "Radio", label: qsTr("Radio"), model: radio },
        { title: "Podcasts", label: qsTr("Podcasts"), model: webPodcasts },
        { title: "Nieuws", label: qsTr("Nieuws"), model: webNews },
        { title: "Media", label: qsTr("Media"), model: appModel }
    ]
    // Layout: "classic" (rijen onder elkaar) of "tabs" (tabbladen bovenin, rijen die zijwaarts scrollen).
    // Per profiel instelbaar via Instellingen; in de rooktests gebruiken alleen de tabs-tests de tabbladen.
    property string layoutName: appModel.setting("layout", "classic")
    readonly property bool tabsMode: (selfTest === "" || selfTest.indexOf("tabs") === 0) && layoutName === "tabs"
    property int tab: 0
    readonly property var tabDefs: [
        { id: "foryou", label: qsTr("Voor jou"), titles: ["Verder kijken", "Nu op tv", "Vaak gebruikt"] },
        { id: "tv", label: qsTr("TV & Gids"), titles: ["Nu op tv"] },
        { id: "apps", label: qsTr("Apps"), titles: ["Video", "Media"] },
        { id: "listen", label: qsTr("Luisteren"), titles: ["Radio", "Muziek", "Podcasts"] },
        { id: "news", label: qsTr("Nieuws"), titles: ["Nieuws"] }
    ]
    readonly property var tabPool: allSectionDefs.concat([{ title: "Verder kijken", label: qsTr("Verder kijken"), model: resume }])
    function sectionShown(d) {
        return d.model === usage ? usage.count >= 3 && (selfTest === "" || selfTest === "usage")
             : d.model === nowTv ? nowTv.count > 0 && (selfTest === "" || selfTest === "nowtv" || selfTest.indexOf("tabs") === 0)
             : d.model.count > 0
    }
    function tabSections(t) {
        const pick = (titles) => titles.map(n => tabPool.find(d => d.title === n)).filter(d => d && sectionShown(d))
        const list = pick(t.titles)
        // Voor jou nog leeg (geen geschiedenis, geen favoriete zenders): dan de eigen tegels
        return list.length === 0 && t.id === "foryou" ? pick(["Media"]) : list
    }
    // Tabs zonder inhoud verdwijnen (TV & Gids blijft: daar staan de knoppen Live tv en Tv-gids)
    readonly property var visibleTabs: tabDefs.filter(t => t.id === "foryou" || t.id === "tv" || tabSections(t).length > 0)
    readonly property var sectionDefs: {
        if (tabsMode) {
            const t = visibleTabs[Math.min(tab, visibleTabs.length - 1)]
            return t ? tabSections(t) : []
        }
        const order = appModel.sectionOrder
        // (In de rooktests alleen bij de test "usage", zodat de rijvolgorde van andere tests klopt)
        const defs = allSectionDefs.filter(d => sectionShown(d) && !(d.model === nowTv && selfTest.indexOf("tabs") === 0))
        const rank = (d) => { const i = order.indexOf(d.title); return i >= 0 ? i : 100 + allSectionDefs.indexOf(d) }
        return defs.slice().sort((a, b) => rank(a) - rank(b))
    }
    function selectTab(i) {
        if (i === tab || i < 0 || i >= visibleTabs.length) return
        // De rijen worden opnieuw gemaakt: eerst loslaten, de focus blijft op de tabbalk
        currentSection = null
        tab = i
        page.contentY = 0
    }
    function tabId() {
        const t = visibleTabs[Math.min(tab, visibleTabs.length - 1)]
        return t ? t.id : ""
    }
    // Van de tabbalk naar de inhoud van het tabblad
    function enterTabContent() {
        if (tabId() === "tv") liveButton.forceActiveFocus()
        else focusSection(0, 0, false)
    }
    // Boven de eerste rij: terug naar de tabbalk (of de knoppen op het tv-tabblad)
    function focusTabTop() {
        if (tabId() === "tv") liveButton.forceActiveFocus()
        else tabList.forceActiveFocus()
    }
    function openLiveTv() {
        const go = () => { radioPlayer.stopPlayback(); iptvScreen.open(liveButton) }
        if (parental.needsPin("bigscreen:iptv")) {
            pinPad.ask(qsTr("Live tv is vergrendeld"), qsTr("Vul de pincode van het kinderslot in."), (pin) => { if (!parental.check(pin)) return false; go(); return true }, liveButton)
        } else {
            go()
        }
    }
    function openTvGuide() {
        const go = () => {
            radioPlayer.stopPlayback()
            iptvScreen.open(liveButton)
            if (iptv.sources.length > 0) iptvScreen.openGrid()
        }
        if (parental.needsPin("bigscreen:iptv")) {
            pinPad.ask(qsTr("Live tv is vergrendeld"), qsTr("Vul de pincode van het kinderslot in."), (pin) => { if (!parental.check(pin)) return false; go(); return true }, liveButton)
        } else {
            go()
        }
    }
    function moveSection(model, step) {
        const titles = allSectionDefs.slice().sort((a, b) => {
            const order = appModel.sectionOrder
            const ra = order.indexOf(a.title) >= 0 ? order.indexOf(a.title) : 100 + allSectionDefs.indexOf(a)
            const rb = order.indexOf(b.title) >= 0 ? order.indexOf(b.title) : 100 + allSectionDefs.indexOf(b)
            return ra - rb
        }).map(d => d.title)
        const title = allSectionDefs.find(d => d.model === model).title
        // Over verborgen rijen (lege Verder kijken) heen springen
        const visible = sectionDefs.map(d => d.title)
        const vi = visible.indexOf(title)
        const target = visible[vi + step]
        if (target === undefined) return
        titles.splice(titles.indexOf(title), 1)
        titles.splice(titles.indexOf(target) + (step > 0 ? 1 : 0), 0, title)
        appModel.sectionOrder = titles
        showToast(qsTr("Rij ") + title + (step < 0 ? qsTr(" staat nu hoger") : qsTr(" staat nu lager")))
        Qt.callLater(() => { const sec = sectionFor(model); if (sec) sec.grid.forceActiveFocus() })
    }
    // Categorie van een rij (voor Vaak gebruikt en inklappen)
    function categoryOf(model) {
        return model === appModel ? "media" : model === webVideo ? "video" : model === webMusic ? "muziek"
             : model === webPodcasts ? "podcasts" : model === webNews ? "nieuws" : model === radio ? "radio" : ""
    }
    // Rij ingeklapt? Zelf ingeklapt, of (automatisch) twee weken niet gebruikt
    property int settingsRevision: 0
    Connections { target: appModel; function onSettingsChanged() { root.settingsRevision++ } }
    function isCollapsed(title, model) {
        settingsRevision
        // Lege lijst komt uit het ini-bestand als "ongeldig" terug, één naam als tekst
        const asList = (v) => !v ? [] : typeof v === "string" ? [v] : v
        const manual = asList(appModel.setting("collapsed", []))
        const expanded = asList(appModel.setting("expanded", []))
        if (manual.indexOf(title) >= 0) return true
        if (expanded.indexOf(title) >= 0 || model === usage || model === appModel || model === nowTv) return false
        return appModel.setting("autoCollapse", true) && usage.daysUnused(categoryOf(model)) > 14
    }
    function toggleCollapsed(model) {
        const title = allSectionDefs.find(d => d.model === model).title
        const now = isCollapsed(title, model)
        const asList = (v) => !v ? [] : typeof v === "string" ? [v] : v
        let manual = asList(appModel.setting("collapsed", [])).filter(t => t !== title)
        let expanded = asList(appModel.setting("expanded", [])).filter(t => t !== title)
        if (now) expanded.push(title); else manual.push(title)
        appModel.setSetting("collapsed", manual)
        appModel.setSetting("expanded", expanded)
        showToast(now ? qsTr("Rij ") + title + qsTr(" staat weer open") : qsTr("Rij ") + title + qsTr(" is ingeklapt (kies hem om te openen)"))
    }
    function sectionMoveEntries(model) {
        const visible = sectionDefs.map(d => d.model)
        const i = visible.indexOf(model)
        const entries = []
        if (i > 0) entries.push({ label: qsTr("Rij omhoog verplaatsen"), action: "section-up" })
        if (i >= 0 && i < visible.length - 1) entries.push({ label: qsTr("Rij omlaag verplaatsen"), action: "section-down" })
        const def = allSectionDefs.find(d => d.model === model)
        if (def && model !== usage) entries.push({ label: isCollapsed(def.title, model) ? qsTr("Rij uitklappen") : qsTr("Rij inklappen"), action: "section-collapse" })
        return entries
    }
    readonly property var webModels: [webVideo, webMusic, webPodcasts, webNews]
    // Rij "Verder kijken" verschijnt of verdwijnt: de rijen worden opnieuw gemaakt, focus terugzetten
    onSectionDefsChanged: Qt.callLater(() => {
        if (!frameScreen.visible && !iptvScreen.visible && !libraryScreen.visible && !radioPlayer.visible && root.currentSection === null
                && !quickRow.activeFocus && !tabList.activeFocus && !liveButton.activeFocus && !guideButton.activeFocus
                && !menu.visible && !welcomeOverlay.visible && !addOverlay.visible) {
            focusSection(0, 0, false)
        }
    })

    // Rij en tegel die nu focus hebben
    property Item currentSection: null
    readonly property var focusModel: currentSection !== null && currentSection.grid.activeFocus ? currentSection.model : null
    readonly property int focusRow: focusModel ? currentSection.grid.currentIndex : -1
    readonly property bool focusIsWeb: focusModel ? focusModel !== appModel : false

    // Muis telt alleen als hij echt bewogen is, niet als tegels onder een stilstaande
    // cursor doorschuiven tijdens het scrollen met pijltjes of controller
    property double lastMouseMove: 0
    function mouseActive() {
        // Muis boven een volledig scherm (schilderij, live tv, bibliotheek): de tegels
        // eronder mogen dan geen focus pakken, anders werken Esc/B niet meer
        if (frameScreen.visible || iptvScreen.visible || libraryScreen.visible || radioPlayer.visible) {
            return false
        }
        return Date.now() - lastMouseMove < 400
    }

    // Naam en soort van de tegel die nu focus heeft (voor het grote tekstvlak bovenaan)
    readonly property string focusName: focusModel ? focusModel.nameAt(focusRow)
                                       : quickRow.activeFocus ? quickModel.get(quickRow.currentIndex).label : ""
    readonly property string focusKind: focusModel ? currentSection.title
                                       : quickRow.activeFocus ? quickModel.get(quickRow.currentIndex).about : ""
    // (revision in de binding: opnieuw opvragen zodra een logo binnen is of wijzigt)
    readonly property string focusIconRaw: focusModel ? (focusModel.revision, focusModel.iconAt(focusRow)) : ""
    // Pictogrammen uit het lettertype (Films, Series) niet als afbeelding laden
    readonly property string focusIcon: focusIconRaw.startsWith("glyph:") ? "" : focusIconRaw
    // Achtergrondkleur: de kleur van het logo van de dienst met focus
    readonly property color accent: focusIsWeb && focusRow >= 0 ? (focusModel.revision, focusModel.accentAt(focusRow)) : "#1d3557"

    MouseArea {
        anchors.fill: parent
        z: 1000
        acceptedButtons: Qt.NoButton
        hoverEnabled: true
        onPositionChanged: root.lastMouseMove = Date.now()
    }
    readonly property bool heroShown: appModel.showHero

    // Achtergrond: neemt de kleur van de gekozen streamingdienst aan, naar onder donker
    Rectangle {
        anchors.fill: parent
        visible: appModel.background === ""
        color: root.accent
        Behavior on color { ColorAnimation { duration: 450 } }
        // Meebewegende achtergrond: het logo van de gekozen tegel, groot en wazig (zoals Apple TV)
        Image {
            id: ambientIcon
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: -parent.width * 0.05
            width: parent.width * 0.55
            height: width
            source: root.focusIcon
            fillMode: Image.PreserveAspectFit
            sourceSize: Qt.size(256, 256)
            asynchronous: true
            visible: false
        }
        // Eén keer vervagen en als plaatje bewaren (layer): anders rekent de vervaging bij elk videobeeld mee.
        // Verborgen zolang een volledig scherm (tv, bibliotheek, schilderij, radio) open is.
        Item {
            anchors.fill: ambientIcon
            visible: !(iptvScreen.visible || libraryScreen.visible || frameScreen.visible || radioPlayer.visible)
            layer.enabled: visible
            opacity: root.focusIcon !== "" ? 0.28 : 0
            Behavior on opacity { NumberAnimation { duration: 450 } }
            MultiEffect {
                anchors.fill: parent
                source: ambientIcon
                blurEnabled: true
                blur: 1.0
                blurMax: 32
            }
        }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#300b0c0f" }
                GradientStop { position: 0.45; color: "#b00b0c0f" }
                GradientStop { position: 1.0; color: "#f20b0c0f" }
            }
        }
    }

    function sectionFor(model) {
        for (let i = 0; i < sections.count; ++i) {
            const item = sections.itemAt(i)
            if (item && item.model === model) {
                return item
            }
        }
        return null
    }

    // Zender kijken vanuit Nu op tv, een herinnering of het zoekscherm (met kinderslot)
    function watchChannel(url, back) {
        const playChannel = () => {
            radioPlayer.stopPlayback()
            iptvScreen.open(back || mediaGrid())
            let row = iptv.rowOf(url)
            if (row < 0) row = iptv.showChannel(url)
            if (row >= 0) iptvScreen.play(row)
        }
        if (parental.needsPin("bigscreen:iptv")) {
            pinPad.ask(qsTr("Live tv is vergrendeld"), qsTr("Vul de pincode van het kinderslot in."), (pin) => { if (!parental.check(pin)) return false; playChannel(); return true }, back || quickRow)
        } else {
            playChannel()
        }
    }

    function mediaGrid() {
        const media = sectionFor(appModel)
        return media ? media.grid : quickRow
    }

    // Focus naar rij i; lege (verborgen) rijen worden overgeslagen
    function focusSection(i, column, fromBelow) {
        while (i >= 0 && i < sections.count) {
            const item = sections.itemAt(i)
            if (item && item.enter(column, fromBelow)) {
                return
            }
            i += fromBelow ? -1 : 1
        }
        if (i < 0) {
            if (root.tabsMode) root.focusTabTop()
            else quickRow.forceActiveFocus()
            page.contentY = 0
        }
    }

    // Pagina zo scrollen dat de gekozen tegel (en bij de eerste rij de titel) in beeld is
    function scrollTo(section) {
        const g = section.grid
        const row = section.strip ? 0 : Math.floor(Math.max(0, g.currentIndex) / root.columns)
        const top = row === 0 ? section.y : section.y + g.y + g.topMargin + row * g.cellHeight - root.unit * 2
        const bottom = section.y + g.y + g.topMargin + (row + 1) * g.cellHeight + root.unit * 2
        if (top < page.contentY) {
            page.contentY = Math.max(0, top)
        } else if (bottom > page.contentY + page.height) {
            page.contentY = Math.max(0, Math.min(page.contentHeight - page.height, bottom - page.height))
        }
    }

    function launch(model, row, pinOk) {
        if (row < 0) {
            return
        }
        // Vaak gebruikt: de echte tegel in zijn eigen rij openen
        if (model === usage) {
            const cat = usage.categoryAt(row)
            const src = usage.sourceModel(cat)
            const r = usage.sourceRow(cat, usage.pathAt(row))
            if (src && r >= 0) launch(src, r, pinOk)
            return
        }
        // Nu op tv: meteen de zender kijken
        if (model === nowTv) {
            console.info("Now on tv:", nowTv.nameAt(row))
            watchChannel(nowTv.urlAt(row), sectionFor(nowTv) ? sectionFor(nowTv).grid : quickRow)
            return
        }
        if (model === radio) {
            if (radio.isAddTile(row)) {
                openRadioSearch()
                return
            }
            // Radio speelt in Kijkluister zelf
            usage.record("radio", radio.pathAt(row))
            const section = sectionFor(radio)
            radioPlayer.open(row, section ? section.grid : mediaGrid())
            return
        }
        const path = model.pathAt ? model.pathAt(row) : ""
        const name = model.nameAt(row)
        // Kinderslot: eerst de pincode
        if (!pinOk && parental.needsPin(path)) {
            pinPad.ask(name + qsTr(" is vergrendeld"), qsTr("Vul de pincode van het kinderslot in."),
                       (pin) => { if (!parental.check(pin)) return false; root.launch(model, row, true); return true },
                       currentSection ? currentSection.grid : quickRow)
            return
        }
        // Een andere app of film: radio stoppen (bij het schilderij mag hij doorspelen)
        if (radioPlayer.row >= 0 && path !== "bigscreen:frame") {
            radioPlayer.stopPlayback()
        }
        if (path !== "bigscreen:radio-now" && path !== "bigscreen:resume" && categoryOf(model) !== "") {
            usage.record(categoryOf(model), path)
        }
        if (model.launch(row)) {
            // App of website voorop: Home-knop en Home-toets brengen je terug
            if (!path.startsWith("bigscreen:")) {
                homeBridge.externalLaunched()
                // Blijft staan tot het venster echt open is (Kijkluister gaat dan naar de achtergrond)
                showToast(name + qsTr(" wordt geopend…"), 12000)
                launchSlowTimer.name = name
                launchSlowTimer.restart()
            }
        } else {
            showToast(name + qsTr(" kon niet worden geopend. Probeer het opnieuw."), 5000)
        }
    }

    function showToast(message, ms) {
        toast.text = message
        toastTimer.interval = ms || 3500
        toastTimer.restart()
    }

    // Duurt het openen lang en staat Kijkluister nog steeds voorop: een uitweg bieden
    Timer {
        id: launchSlowTimer
        property string name
        interval: 10000
        onTriggered: if (Qt.application.state === Qt.ApplicationActive)
                         root.showToast(name + qsTr(" opent nog niet. Duurt het lang? Kies de tegel nog een keer."), 6000)
    }
    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state !== Qt.ApplicationActive) {
                launchSlowTimer.stop()
                toastTimer.stop()
            }
        }
    }

    function openTileMenu(model, row, focusItem) {
        if (row < 0) {
            return
        }
        editModel = model
        editRow = row
        if (model === radio) {
            const entries = [{ label: qsTr("Luisteren"), action: "radio-play" }, { label: qsTr("Radiozender toevoegen…"), action: "radio-add" }]
            for (const dir of ["left", "right", "up", "down"]) {
                if (root.moveLimit(model, row, dir) > 0) entries.push({ label: root.moveLabels[dir] + qsTr(" verplaatsen…"), action: "move-" + dir })
            }
            entries.push(...sectionMoveEntries(model))
            if (!radio.isAddTile(row)) entries.push({ label: qsTr("Van startpagina verwijderen"), action: "radio-remove" })
            entries.push(...commonMenuEntries())
            menu.open(model.nameAt(row), entries, focusItem)
            return
        }
        if (model === usage) {
            menu.open(model.nameAt(row), [
                { label: qsTr("Openen"), action: "usage-open" },
                { label: qsTr("Uit Vaak gebruikt halen"), action: "usage-remove" }
            ].concat(sectionMoveEntries(model)).concat(commonMenuEntries()), focusItem)
            return
        }
        if (model === resume) {
            menu.open(model.nameAt(row), [
                { label: qsTr("Verder kijken"), action: "resume-play" },
                { label: qsTr("Uit deze rij halen"), action: "resume-remove" }
            ].concat(sectionMoveEntries(model)).concat(commonMenuEntries()), focusItem)
            return
        }
        const entries = [{ label: qsTr("Pictogram wijzigen…"), action: "icon" }]
        if (model.hasCustomIcon(row)) {
            entries.push({ label: qsTr("Standaardpictogram herstellen"), action: "icon-reset" })
        }
        for (const dir of ["left", "right", "up", "down"]) {
            if (root.moveLimit(model, row, dir) > 0) {
                entries.push({ label: root.moveLabels[dir] + qsTr(" verplaatsen…"), action: "move-" + dir })
            }
        }
        if (parental.enabled && model.pathAt) {
            entries.push(parental.isLocked(model.pathAt(row))
                         ? { label: qsTr("🔓 Kinderslot van deze tegel halen"), action: "parental-unlock-tile" }
                         : { label: qsTr("🔒 Kinderslot op deze tegel"), action: "parental-lock-tile" })
        }
        entries.push(...sectionMoveEntries(model))
        if (model.pathAt && model.pathAt(row) !== "bigscreen:radio-now") {
            entries.push({ label: qsTr("Van startpagina verwijderen"), action: "hide" })
        }
        entries.push(...commonMenuEntries())
        menu.open(model.nameAt(row), entries, focusItem)
    }

    // Onderaan elk tegelmenu: de knoppen die vroeger in de bovenbalk stonden
    // Waar de focus na een menu heen gaat: de tegel waar je was, anders de bovenbalk
    function menuFocus() {
        return currentSection ? currentSection.grid : quickRow
    }

    function commonMenuEntries() {
        return [
            { label: qsTr("🌙  Slaaptimer…") + (sleepTimer.active ? qsTr("  (nog %1)").arg(sleepTimer.remainingText) : ""), action: "sleep-menu" },
            { label: qsTr("⚙  Instellingen…"), action: "personalize-menu" },
            { label: qsTr("Annuleren"), action: "" }
        ]
    }

    readonly property var moveLabels: ({ left: qsTr("Naar links"), right: qsTr("Naar rechts"), up: qsTr("Naar boven"), down: qsTr("Naar onder") })

    // Hoeveel plaatsen een tegel maximaal kan schuiven (omhoog/omlaag: in rijen)
    function moveLimit(model, row, dir) {
        const last = model.count - 1
        if (dir === "left") return row
        if (dir === "right") return last - row
        if (dir === "up") return Math.floor(row / root.columns)
        return Math.ceil((last - row) / root.columns)
    }

    function moveTile(model, row, dir, places) {
        const step = dir === "left" ? -1 : dir === "right" ? 1 : dir === "up" ? -root.columns : root.columns
        const to = Math.max(0, Math.min(model.count - 1, row + step * places))
        if (to === row) {
            return
        }
        model.move(row, to)
        const section = sectionFor(model)
        if (section) {
            section.grid.currentIndex = to
            section.grid.forceActiveFocus()
        }
    }

    // --- Instellingen: één menu met zes groepen (zie docs/volgende-versie.md) ---
    // Toestand altijd als "Naam:  waarde"; "Terug" gaat naar de groepenlijst, Esc sluit alles.
    property string settingsGroup: ""

    function onOff(v) { return v ? qsTr("aan") : qsTr("uit") }
    function escModeLabel() {
        const m = systemTools.globalSetting("escMode", "hold")
        return m === "double" ? qsTr("twee keer indrukken") : m === "direct" ? qsTr("direct") : qsTr("vasthouden")
    }

    function openPersonalizeMenu() {
        // Met kinderslot aan zijn de instellingen alleen met de pincode te openen
        if (parental.enabled && !parental.unlocked) {
            withPin(qsTr("Instellingen"), () => Qt.callLater(() => root.openSettings("")))
            return
        }
        openSettings("")
    }

    function openSettings(group) {
        settingsGroup = group
        console.info("Settings group", group === "" ? "top" : group)
        const back = { label: qsTr("Terug"), action: "personalize-menu" }
        if (group === "") {
            menu.open(qsTr("Instellingen"), [
                { label: qsTr("Startpagina…"), action: "grp:home" },
                { label: qsTr("Uiterlijk…"), action: "grp:look" },
                { label: qsTr("Gezin en veiligheid…"), action: "grp:family" },
                { label: qsTr("Geluid en apparaten…"), action: "grp:devices" },
                { label: qsTr("Opnames en sleutels…"), action: "grp:tv" },
                { label: qsTr("Onderhoud en hulp…"), action: "grp:system" },
                { label: qsTr("Sluiten"), action: "" }
            ], root.menuFocus(), qsTr("Kies een onderwerp"))
        } else if (group === "home") {
            const entries = [{ label: qsTr("Tegel toevoegen…"), action: "add" }]
            let hidden = appModel.hiddenCount
            for (const m of webModels) hidden += m.hiddenCount
            hidden += radio.hiddenCount
            if (hidden > 0) entries.push({ label: qsTr("Verwijderde tegels terugzetten (") + hidden + ")", action: "unhide" })
            entries.push({ label: qsTr("Weinig gebruikte rijen inklappen: ") + onOff(appModel.setting("autoCollapse", true)), action: "auto-collapse" })
            entries.push({ label: qsTr("Radio hervatten bij opstarten: ") + onOff(appModel.setting("resumeRadio", false)), action: "resume-radio" })
            entries.push({ label: qsTr("Weer: ") + (weather.enabled ? (weather.place || qsTr("aan")) : qsTr("uit")), action: "weather" })
            entries.push({ label: qsTr("Welkomsthulp: diensten en apps kiezen…"), action: "welcome" })
            entries.push(back)
            menu.open(qsTr("Startpagina"), entries, root.menuFocus())
        } else if (group === "look") {
            menu.open(qsTr("Uiterlijk"), [
                { label: qsTr("Layout: ") + (root.layoutName === "tabs" ? qsTr("tabbladen") : qsTr("klassiek")), action: "layout" },
                { label: qsTr("Achtergrond wijzigen…"), action: "background-menu" },
                { label: qsTr("Tekstgrootte: ") + (appModel.fontScale > 1 ? qsTr("groter") : qsTr("normaal")), action: "font-scale" },
                { label: qsTr("Grote naam en logo linksboven: ") + (appModel.showHero ? qsTr("tonen") : qsTr("verbergen")), action: "toggle-hero" },
                { label: qsTr("Nachtmodus: ") + nightModeLabel(), action: "night" },
                { label: qsTr("Sfeerstand (Schilderij als je niets doet): ") + ambientLabel(), action: "ambient" },
                { label: "Taal / Language: " + (uiLanguage === "en" ? "English" : "Nederlands"), action: "language" },
                back
            ], root.menuFocus())
        } else if (group === "family") {
            menu.open(qsTr("Gezin en veiligheid"), [
                { label: qsTr("Profielen: ") + profiles.currentName, action: "profiles" },
                { label: qsTr("Bij opstarten vragen wie er kijkt: ") + onOff(systemTools.globalSetting("askProfile", true)), action: "ask-profile" },
                { label: qsTr("Kinderslot: ") + onOff(parental.enabled), action: "parental" },
                { label: qsTr("Kijktijd per dag") + (profiles.kids ? "" : qsTr(" (kinderprofiel)")) + ": " + kidsLimitLabel(), action: "kids-limit" },
                back
            ], root.menuFocus(), qsTr("Met kinderslot aan vragen deze instellingen de pincode."))
        } else if (group === "devices") {
            menu.open(qsTr("Geluid en apparaten"), [
                { label: qsTr("Geluid via: ") + (systemTools.audioOutputName() || qsTr("standaard")), action: "audio" },
                { label: qsTr("Tv-afstandsbediening (HDMI-CEC)…"), action: "cec" },
                { label: qsTr("Telefoon als afstandsbediening: ") + onOff(remote.enabled), action: "remote" },
                { label: qsTr("Terug naar de startpagina met Esc: ") + escModeLabel(), action: "esc-mode" },
                { label: qsTr("Windows: wifi, bluetooth, beeld en geluid…"), action: "windows" },
                back
            ], root.menuFocus())
        } else if (group === "tv") {
            menu.open(qsTr("Opnames en sleutels"), [
                { label: qsTr("Opnames opruimen…"), action: "cleanup-settings" },
                { label: qsTr("Sleutels voor films en foto's (TMDB, Unsplash)…"), action: "keys" },
                back
            ], root.menuFocus())
        } else {
            menu.open(qsTr("Onderhoud en hulp"), [
                { label: qsTr("Zoeken naar een nieuwe versie"), action: "update-check" },
                { label: qsTr("Release-info (versie ") + updater.current + ")", action: "release-info" },
                { label: qsTr("Back-up maken of terugzetten…"), action: "backup" },
                { label: qsTr("Starten bij opstarten van Windows: ") + onOff(systemTools.autostart), action: "autostart" },
                { label: qsTr("Sneltoetsen en hulp"), action: "help-open" },
                { label: qsTr("Logbestand delen (voor hulp aan de maker)"), action: "share-log" },
                back
            ], root.menuFocus(), "Kijkluister " + updater.current)
        }
    }

    // Opent de groep waar je net in zat (na een schakelaar blijft het menu zo open)
    function openMoreMenu() {
        openSettings(settingsGroup)
    }

    function ambientLabel() {
        const m = appModel.setting("ambientMinutes", 10)
        return m === 0 ? qsTr("uit") : qsTr("na ") + m + qsTr(" minuten")
    }
    function kidsLimitLabel() {
        const m = parental.kidsMinutes()
        return m === 0 ? qsTr("onbeperkt") : m < 60 ? m + qsTr(" minuten") : (m / 60) + qsTr(" uur")
    }
    function openAudioMenu(focusItem) {
        const outs = systemTools.audioOutputs()
        console.info("Audio outputs", outs.length, systemTools.audioOutputName())
        if (outs.length === 0) {
            showToast(qsTr("Geen geluidsuitgangen gevonden"), 4000)
            return
        }
        menu.open(qsTr("Geluid via"), outs.map(o => ({ label: (o.isDefault ? "✓  " : "      ") + o.name, action: o.isDefault ? "" : "audio-set:" + o.id }))
                  .concat([{ label: qsTr("Windows-instellingen voor geluid…"), action: "uri:ms-settings:sound" }, { label: qsTr("Sluiten"), action: "" }]),
                  focusItem, qsTr("Geldt voor alles: Kijkluister, Netflix, YouTube en de rest."))
    }

    function nightModeLabel() {
        const m = appModel.setting("night", "auto")
        return m === "on" ? qsTr("altijd aan") : m === "off" ? qsTr("uit") : qsTr("automatisch (22–7 uur)")
    }

    // --- Profielen ---
    property bool pendingKidsProfile: false
    function openProfileMenu() {
        const entries = profiles.list.map(p => ({
            label: (p.current ? "✓  " : "      ") + p.name + (p.kids ? qsTr("  (kind)") : ""),
            action: p.current ? "" : "profile-switch:" + p.id }))
        if (!profiles.kids) {
            entries.push({ label: qsTr("Profiel toevoegen…"), action: "profile-add" })
            entries.push({ label: qsTr("Kinderprofiel toevoegen…"), action: "profile-add-kids" })
            for (const p of profiles.list) {
                if (p.id !== "" && !p.current) entries.push({ label: qsTr("Profiel %1 verwijderen").arg(p.name), action: "profile-remove:" + p.id })
            }
        }
        entries.push({ label: qsTr("Sluiten"), action: "" })
        menu.open(qsTr("Profielen"), entries, root.menuFocus(),
                  qsTr("Nu: ") + profiles.currentName + qsTr(". Elk profiel heeft eigen tegels, rijen, radiozenders en Vaak gebruikt. Wisselen herstart Kijkluister."))
    }

    function openParentalMenu() {
        if (!parental.enabled) {
            menu.open(qsTr("Kinderslot"), [
                { label: qsTr("Kinderslot aanzetten…"), action: "parental-set" },
                { label: qsTr("Sluiten"), action: "" }
            ], root.menuFocus(), qsTr("Met een eigen pincode vergrendel je Live tv, Films, Series en tegels die je kiest. Zenders voor volwassenen zijn dan altijd vergrendeld."))
            return
        }
        menu.open(qsTr("Kinderslot"), [
            { label: qsTr("Nu vergrendelen"), action: "parental-lock-now" },
            { label: qsTr("Pincode wijzigen…"), action: "parental-change" },
            { label: qsTr("Kinderslot uitzetten…"), action: "parental-off" },
            { label: qsTr("Sluiten"), action: "" }
        ], root.menuFocus(), (parental.unlocked ? qsTr("Nu ontgrendeld (een uur na de pincode). ") : "")
                     + qsTr("Vergrendelde tegels: kies een tegel, druk op Menu en kies Kinderslot."))
    }

    function askNewPin() {
        pinPad.ask(qsTr("Nieuwe pincode"), qsTr("Kies vier cijfers die kinderen niet weten."), (first) => {
            Qt.callLater(() => pinPad.ask(qsTr("Pincode herhalen"), qsTr("Vul dezelfde vier cijfers nog een keer in."), (second) => {
                if (second !== first) {
                    root.showToast(qsTr("De pincodes waren niet gelijk. Probeer het opnieuw."), 5000)
                    return true
                }
                const wasOn = parental.enabled
                parental.setPin(first)
                root.showToast(wasOn ? qsTr("Nieuwe pincode ingesteld") : qsTr("Kinderslot staat aan: Live tv, Films en Series vragen nu om de pincode"), 6000)
                return true
            }, root.menuFocus()))
            return true
        }, root.menuFocus())
    }

    function withPin(title, action) {
        pinPad.ask(title, qsTr("Vul de pincode van het kinderslot in."), (pin) => {
            if (!parental.check(pin)) return false
            action(pin)
            return true
        }, currentSection ? currentSection.grid : quickRow)
    }

    function openCleanupMenu() {
        const days = recorder.keepDays
        const gb = recorder.minFreeGb
        menu.open(qsTr("Opnames opruimen"), [
            { label: qsTr("Bewaren: ") + (days > 0 ? days + qsTr(" dagen") : "altijd") + qsTr("  (wijzigen)"), action: "cleanup-days" },
            { label: qsTr("Ruimte vrijhouden: ") + (gb > 0 ? gb + qsTr(" GB") : qsTr("uit")) + qsTr("  (wijzigen)"), action: "cleanup-space" },
            { label: qsTr("Nu opruimen…"), action: "cleanup-now" },
            { label: qsTr("Sluiten"), action: "" }
        ], root.menuFocus(), recorder.diskText + qsTr(". Kijkluister vraagt het altijd eerst voordat er iets wordt gewist."))
    }

    function askCleanup(manual) {
        const list = recorder.cleanupCandidates()
        if (list.length === 0) {
            if (manual) showToast(qsTr("Er hoeft niets opgeruimd te worden"), 4000)
            return
        }
        let bytes = 0
        for (const r of list) bytes += r.size
        cleanupPaths = list.map(r => r.path)
        const sizeText = bytes >= 1e8 ? (bytes / 1e9).toFixed(1) + qsTr(" GB") : Math.max(1, Math.round(bytes / 1e6)) + qsTr(" MB")
        const names = list.slice(0, 3).map(r => "• " + r.name + " (" + r.date + ")").join("\n")
        menu.open(list.length === 1 ? qsTr("1 opname opruimen?") : list.length + qsTr(" opnames opruimen?"), [
            { label: qsTr("Wissen (") + sizeText + ")", action: "cleanup-confirm" },
            { label: qsTr("Laten staan"), action: "" }
        ], currentSection ? currentSection.grid : quickRow,
           qsTr("Reden: ") + list[0].reason + ".\n" + names + (list.length > 3 ? qsTr("\n… en ") + (list.length - 3) + qsTr(" meer") : ""))
    }
    property var cleanupPaths: []

    function openCecMenu() {
        const entries = []
        if (!cec.installed) {
            entries.push({ label: qsTr("libCEC ophalen en installeren"), action: "cec-install" })
        } else {
            entries.push({ label: qsTr("Opnieuw verbinden"), action: "cec-start" })
        }
        entries.push({ label: qsTr("Sluiten"), action: "" })
        menu.open(qsTr("Tv-afstandsbediening"), entries, root.menuFocus(),
                  (cec.installed ? cec.status + ". " : "")
                  + qsTr("Bedien Kijkluister met de afstandsbediening van je tv. Nodig: een USB-CEC-adapter van Pulse-Eight tussen computer en tv (HDMI)."))
    }

    function openSleepMenu(focusItem) {
        const entries = [15, 30, 45, 60, 90].map(m => ({ label: m + qsTr(" minuten"), action: "sleep-" + m }))
        if (sleepTimer.active) {
            entries.unshift({ label: qsTr("Slaaptimer uitzetten"), action: "sleep-0" })
        }
        entries.push({ label: qsTr("Annuleren"), action: "" })
        menu.open(qsTr("Slaaptimer"), entries, focusItem || quickRow,
                  sleepTimer.active ? qsTr("Nog %1. Daarna stoppen radio, tv en films vanzelf.").arg(sleepTimer.remainingText)
                                    : qsTr("Radio, tv en films stoppen vanzelf na de gekozen tijd."))
    }

    function handleMenuAction(action) {
        if (action === "background-menu") {
            const bg = [
                { label: qsTr("Gratis foto zoeken…"), action: "background-photo" },
                { label: qsTr("Eigen afbeelding kiezen…"), action: "background" }
            ]
            if (appModel.background !== "") bg.push({ label: qsTr("Standaardachtergrond herstellen"), action: "background-reset" })
            bg.push({ label: qsTr("Annuleren"), action: "" })
            menu.open(qsTr("Achtergrond wijzigen"), bg, root.menuFocus(),
                      photos.source === "Unsplash" ? qsTr("Gratis foto's van Unsplash.") : qsTr("Gratis foto's met een vrije licentie (Openverse)."))
        } else if (action === "background-photo") {
            openPicker("photo", qsTr("Gratis foto als achtergrond"), root.menuFocus(), "landschap")
        } else if (action === "layout") {
            const next = root.layoutName === "tabs" ? "classic" : "tabs"
            // De rijen worden opnieuw gemaakt: eerst loslaten, daarna de focus expliciet zetten
            root.currentSection = null
            appModel.setSetting("layout", next)
            root.layoutName = next
            root.tab = 0
            page.contentY = 0
            root.showToast(next === "tabs" ? qsTr("Layout: tabbladen") : qsTr("Layout: klassiek"))
            Qt.callLater(() => { if (root.tabsMode) tabList.forceActiveFocus(); else root.focusSection(0, 0, false) })
        } else if (action === "release-info") {
            releaseInfo.open(root.menuFocus())
        } else if (action === "section-collapse") {
            toggleCollapsed(editModel)
        } else if (action === "usage-remove") {
            usage.remove(editRow)
        } else if (action === "usage-open") {
            launch(usage, editRow)
        } else if (action === "profiles") {
            openProfileMenu()
        } else if (action.startsWith("profile-switch:")) {
            const id = action.substring(15)
            if (profiles.kids && parental.enabled) {
                withPin(qsTr("Ander profiel kiezen"), () => profiles.switchTo(id))
            } else {
                profiles.switchTo(id)
            }
        } else if (action === "profile-add" || action === "profile-add-kids") {
            pendingKidsProfile = action === "profile-add-kids"
            if (pendingKidsProfile && !parental.enabled) {
                showToast(qsTr("Stel eerst een pincode in voor het kinderslot"), 5000)
                askNewPin()
                return
            }
            openPicker("name", pendingKidsProfile ? qsTr("Naam van het kinderprofiel") : qsTr("Naam van het nieuwe profiel"), root.menuFocus())
        } else if (action.startsWith("profile-remove:")) {
            const pid = action.substring(15)
            const pname = profiles.nameOf(pid)
            profiles.remove(pid)
            showToast(qsTr("Profiel %1 verwijderd").arg(pname))
        } else if (action === "language") {
            menu.open("Taal / Language", [
                { label: (uiLanguage === "nl" ? "✓  " : "      ") + "Nederlands", action: uiLanguage === "nl" ? "" : "language-set:nl" },
                { label: (uiLanguage === "en" ? "✓  " : "      ") + "English", action: uiLanguage === "en" ? "" : "language-set:en" }
            ], root.menuFocus(), uiLanguage === "en" ? "Kijkluister restarts in the chosen language." : "Kijkluister start opnieuw in de gekozen taal.")
        } else if (action.startsWith("language-set:")) {
            systemTools.setGlobalSetting("language", action.substring(13))
            console.info("Language set", action.substring(13))
            systemTools.restart()
        } else if (action === "autostart") {
            systemTools.autostart = !systemTools.autostart
            showToast(systemTools.autostart ? qsTr("Kijkluister start voortaan met Windows") : qsTr("Kijkluister start niet meer met Windows"), 4000)
            openMoreMenu()
        } else if (action === "audio") {
            openAudioMenu(root.menuFocus())
        } else if (action.startsWith("audio-set:")) {
            const ok = systemTools.setAudioOutput(action.substring(10))
            showToast(ok ? qsTr("🔊 Geluid via ") + systemTools.audioOutputName() : qsTr("Wisselen lukte niet; kies het in Windows-instellingen → Geluid"), 4000)
        } else if (action === "remote") {
            remoteOverlay.open(root.menuFocus())
        } else if (action === "ambient") {
            const opts = [0, 5, 10, 20, 30]
            const cur = appModel.setting("ambientMinutes", 10)
            appModel.setSetting("ambientMinutes", opts[(opts.indexOf(cur) + 1) % opts.length])
            openMoreMenu()
        } else if (action === "ask-profile") {
            systemTools.setGlobalSetting("askProfile", !systemTools.globalSetting("askProfile", true))
            openMoreMenu()
        } else if (action === "kids-limit") {
            if (!profiles.kids) {
                showToast(qsTr("Stel dit in vanuit het kinderprofiel (met de pincode)"), 5000)
                return
            }
            withPin(qsTr("Kijktijd instellen"), () => Qt.callLater(() => menu.open(qsTr("Kijktijd per dag"), [0, 30, 60, 90, 120, 180].map(m => ({
                label: (m === parental.kidsMinutes() ? "✓  " : "      ") + (m === 0 ? qsTr("Onbeperkt") : m < 60 ? m + qsTr(" minuten") : (m / 60) + qsTr(" uur")),
                action: "kids-limit-" + m })), root.menuFocus(), qsTr("Daarna vraagt Kijkluister de pincode."))))
        } else if (action.startsWith("kids-limit-")) {
            parental.setKidsMinutes(parseInt(action.substring(11)))
            kidsTimer.check()
            showToast(qsTr("Kijktijd: ") + kidsLimitLabel())
        } else if (action === "backup") {
            const files = systemTools.backups()
            const entries = [{ label: qsTr("Back-up maken (op het bureaublad)"), action: "backup-make" }]
            for (const f of files.slice(0, 6)) entries.push({ label: qsTr("Terugzetten: ") + f.replace(qsTr("Kijkluister-backup "), "").replace(".ini", ""), action: "backup-restore:" + f })
            entries.push({ label: qsTr("Sluiten"), action: "" })
            menu.open(qsTr("Back-up"), entries, root.menuFocus(), qsTr("Bevat zenders, profielen, tegels en instellingen, ook inloggegevens van je tv-aanbieder. Bewaar hem goed."))
        } else if (action === "backup-make") {
            const name = systemTools.makeBackup()
            showToast(name ? qsTr("Back-up op het bureaublad: ") + name : qsTr("Back-up maken lukte niet"), 6000)
        } else if (action.startsWith("backup-restore:")) {
            const f = action.substring(15)
            menu.open(qsTr("Back-up terugzetten?"), [
                { label: qsTr("Terugzetten en opnieuw starten"), action: "backup-restore-yes:" + f },
                { label: qsTr("Annuleren"), action: "" }
            ], root.menuFocus(), qsTr("De huidige instellingen worden vervangen door ") + f + ".")
        } else if (action.startsWith("backup-restore-yes:")) {
            if (!systemTools.restoreBackup(action.substring(19))) showToast(qsTr("Dit bestand is geen geldige back-up"), 5000)
        } else if (action === "share-log") {
            const name = systemTools.shareLog()
            showToast(name ? qsTr("Logbestand op het bureaublad: ") + name : qsTr("Logbestand maken lukte niet"), 6000)
        } else if (action === "night") {
            const order = ["auto", "on", "off"]
            appModel.setSetting("night", order[(order.indexOf(appModel.setting("night", "auto")) + 1) % 3])
            openMoreMenu()
        } else if (action === "resume-radio") {
            appModel.setSetting("resumeRadio", !appModel.setting("resumeRadio", false))
            openMoreMenu()
        } else if (action === "auto-collapse") {
            appModel.setSetting("autoCollapse", !appModel.setting("autoCollapse", true))
            openMoreMenu()
        } else if (action === "section-up" || action === "section-down") {
            moveSection(editModel, action === "section-up" ? -1 : 1)
        } else if (action === "sleep-menu") {
            openSleepMenu(currentSection ? currentSection.grid : quickRow)
        } else if (action === "personalize-menu") {
            openSettings("")
        } else if (action.startsWith("grp:")) {
            openSettings(action.substring(4))
        } else if (action === "keys") {
            keysOverlay.open(root.menuFocus())
        } else if (action === "help-open") {
            helpOverlay.open(root.menuFocus())
        } else if (action === "esc-mode") {
            const cur = systemTools.globalSetting("escMode", "hold")
            menu.open(qsTr("Terug naar de startpagina met Esc"), [
                { label: (cur === "hold" ? "✓  " : "      ") + qsTr("Esc vasthouden (standaard)"), action: "esc-set:hold" },
                { label: (cur === "double" ? "✓  " : "      ") + qsTr("Esc twee keer snel indrukken"), action: "esc-set:double" },
                { label: (cur === "direct" ? "✓  " : "      ") + qsTr("Esc direct (sluit de app meteen)"), action: "esc-set:direct" },
                { label: qsTr("Terug"), action: "grp:devices" }
            ], root.menuFocus(), qsTr("Geldt voor apps en websites die Kijkluister opent. Bij 'direct' verlaat Esc ook geen volledig scherm meer in de app zelf."))
        } else if (action.startsWith("esc-set:")) {
            const mode = action.substring(8)
            systemTools.setGlobalSetting("escMode", mode)
            homeBridge.setEscMode(mode)
            showToast(qsTr("Esc: ") + escModeLabel())
            openSettings("devices")
        } else if (action === "windows") {
            menu.open(qsTr("Windows-instellingen"), [
                { label: qsTr("Wifi"), action: "uri:ms-settings:network-wifi" },
                { label: qsTr("Bluetooth (koptelefoon, controller, afstandsbediening)"), action: "uri:ms-settings:bluetooth" },
                { label: qsTr("Beeldscherm"), action: "uri:ms-settings:display" },
                { label: qsTr("Geluid"), action: "uri:ms-settings:sound" },
                { label: qsTr("Alle instellingen"), action: "uri:ms-settings:" },
                { label: qsTr("Sluiten"), action: "" }
            ], root.menuFocus())
        } else if (action.startsWith("uri:")) {
            appModel.openUri(action.substring(4))
        } else if (action === "more") {
            openMoreMenu()
        } else if (action === "parental") {
            openParentalMenu()
        } else if (action === "parental-set") {
            askNewPin()
        } else if (action === "parental-change") {
            withPin(qsTr("Pincode wijzigen"), () => Qt.callLater(askNewPin))
        } else if (action === "parental-off") {
            withPin(qsTr("Kinderslot uitzetten"), (pin) => { parental.disable(pin); root.showToast(qsTr("Kinderslot staat uit")) })
        } else if (action === "parental-lock-now") {
            parental.lockNow()
            showToast(qsTr("Vergrendeld"))
        } else if (action === "parental-lock-tile") {
            parental.setLocked(editModel.pathAt(editRow), true)
            showToast("🔒 " + editModel.nameAt(editRow) + qsTr(" vraagt nu om de pincode"))
        } else if (action === "parental-unlock-tile") {
            const m = editModel, r = editRow
            withPin(qsTr("Kinderslot van %1 halen").arg(m.nameAt(r)), () => { parental.setLocked(m.pathAt(r), false); root.showToast(m.nameAt(r) + qsTr(" is niet meer vergrendeld")) })
        } else if (action === "weather") {
            menu.open(qsTr("Weer"), [
                { label: qsTr("Plaats kiezen…"), action: "weather-place" },
                { label: weather.enabled ? qsTr("Weer niet tonen") : qsTr("Weer tonen"), action: "weather-toggle" },
                { label: qsTr("Sluiten"), action: "" }
            ], root.menuFocus(), weather.available ? weather.place + ": " + weather.summary + " (Open-Meteo)" : qsTr("Het weer staat naast de klok (Open-Meteo)."))
        } else if (action === "weather-place") {
            openPicker("place", qsTr("Plaats voor het weer"), root.menuFocus())
        } else if (action === "weather-toggle") {
            weather.enabled = !weather.enabled
        } else if (action === "cleanup-settings") {
            openCleanupMenu()
        } else if (action === "cleanup-days") {
            menu.open(qsTr("Opnames bewaren"), [0, 7, 14, 30, 60, 90].map(d => ({ label: d === 0 ? qsTr("Altijd") : d + qsTr(" dagen"), action: "cleanup-days-" + d })), root.menuFocus())
        } else if (action.startsWith("cleanup-days-")) {
            recorder.keepDays = parseInt(action.substring(13))
            openCleanupMenu()
        } else if (action === "cleanup-space") {
            menu.open(qsTr("Ruimte vrijhouden op de schijf"), [0, 5, 10, 25, 50].map(g => ({ label: g === 0 ? qsTr("Uit") : g + qsTr(" GB"), action: "cleanup-space-" + g })), quickRow,
                      qsTr("Is de schijf voller, dan stelt Kijkluister voor de oudste opnames te wissen."))
        } else if (action.startsWith("cleanup-space-")) {
            recorder.minFreeGb = parseInt(action.substring(14))
            openCleanupMenu()
        } else if (action === "cleanup-now") {
            askCleanup(true)
        } else if (action === "cleanup-confirm") {
            const n = recorder.cleanup(cleanupPaths)
            showToast(n === 1 ? qsTr("1 opname gewist") : n + qsTr(" opnames gewist"))
        } else if (action === "cec") {
            openCecMenu()
        } else if (action === "cec-install") {
            cec.install()
            showToast(qsTr("libCEC wordt opgehaald…"), 5000)
        } else if (action === "cec-start") {
            cec.start()
        } else if (action === "update-check") {
            updater.check(true)
            showToast(qsTr("Zoeken naar een nieuwe versie…"))
        } else if (action === "update-install") {
            updater.install()
        } else if (action === "update-later") {
            updater.dismiss()
        } else if (action.startsWith("sleep-")) {
            sleepTimer.set(parseInt(action.substring(6)))
        } else if (action === "radio-add") {
            openRadioSearch()
        } else if (action === "radio-remove") {
            const name = radio.nameAt(editRow)
            if (radioPlayer.row === editRow) radioPlayer.stopPlayback()
            radio.remove(editRow)
            showToast(name + qsTr(" verwijderd"))
        } else if (action === "quit") {
            Qt.quit()
        } else if (action === "resume-play") {
            launch(resume, editRow)
        } else if (action === "radio-play") {
            launch(radio, editRow)
        } else if (action === "resume-remove") {
            resume.remove(editRow)
        } else if (action === "icon") {
            imageDialog.purpose = "icon"
            imageDialog.title = qsTr("Pictogram kiezen voor ") + editModel.nameAt(editRow)
            imageDialog.open()
        } else if (action === "icon-reset") {
            editModel.clearCustomIcon(editRow)
        } else if (action.startsWith("move-")) {
            const dir = action.substring(5)
            const section = sectionFor(editModel)
            const grid = section ? section.grid : quickRow
            placesPicker.open(editModel.nameAt(editRow) + ": " + moveLabels[dir].toLowerCase(),
                              dir === "up" || dir === "down" ? "rijen" : "plaatsen",
                              moveLimit(editModel, editRow, dir), grid)
            placesPicker.model = editModel
            placesPicker.row = editRow
            placesPicker.direction = dir
        } else if (action === "font-scale") {
            appModel.fontScale = appModel.fontScale > 1 ? 1.0 : 1.25
            showToast(appModel.fontScale > 1 ? qsTr("Tekst is nu groter") : qsTr("Tekst is weer normaal"))
        } else if (action === "welcome") {
            welcomeOverlay.open(root.menuFocus())
        } else if (action === "toggle-hero") {
            appModel.showHero = !appModel.showHero
        } else if (action === "hide") {
            const name = editModel.nameAt(editRow)
            editModel.hide(editRow)
            showToast(name + qsTr(" verwijderd"))
        } else if (action === "add") {
            addOverlay.open(root.menuFocus())
        } else if (action === "background") {
            imageDialog.purpose = "background"
            imageDialog.title = qsTr("Achtergrond kiezen")
            imageDialog.open()
        } else if (action === "background-reset") {
            appModel.clearBackground()
        } else if (action === "unhide") {
            radio.restoreHidden()
            appModel.restoreHidden()
            for (const m of webModels) {
                m.restoreHidden()
            }
            showToast(qsTr("Alle tegels staan weer op de startpagina"))
        }
    }

    // Schermafbeeldingen van de verbeteringen (alleen via BIGSCREEN_SELFTEST=u-...)
    function runImprovementShot(mode) {
        appModel.fontScale = mode === "u-bigtext" ? 1.25 : 1.0
        iptv.search = ""
        const testSource = () => {
            if (!iptv.sources.some(s => s.name === "Gids-test")) iptv.addSource("m3u-file", "Gids-test", selfTestM3u, "", "")
        }
        const mediaIndex = () => sectionDefs.findIndex(d => d.model === appModel)
        if (mode === "u-library") {
            libraryScreen.open("movies", mediaGrid())
        } else if (mode === "u-home" || mode === "u-bigtext") {
            focusSection(0, 0, false)
        } else if (mode === "u-header") {
            quickRow.currentIndex = 0
            quickRow.forceActiveFocus()
        } else if (mode === "u-quit") {
            let quit = 0
            for (let i = 0; i < quickModel.count; ++i) if (quickModel.get(i).uri === "") quit = i
            quickRow.currentIndex = quit
            quickRow.forceActiveFocus()
            quickRow.trigger(quit)
        } else if (mode === "u-personalize") {
            openPersonalizeMenu()
        } else if (mode === "u-welcome") {
            welcomeOverlay.open(mediaGrid())
        } else if (mode === "u-sources") {
            testSource()
            iptvScreen.open(mediaGrid())
            iptvScreen.openSources()
        } else if (mode === "u-error") {
            iptv.addFreeSources(false)
            iptv.group = "Nederland"
            iptv.search = "NPO 1"
            iptvScreen.autoPlay = true
            iptvScreen.open(mediaGrid())
        } else if (mode === "u-guiderecord") {
            testSource()
            recorder.folder = selfTestRecordDir
            iptv.group = "Alle zenders"
            iptvScreen.autoPlay = true
            iptvScreen.autoGuide = true
            iptvScreen.autoGuideRecord = true
            iptvScreen.guideStart = 3
            iptvScreen.open(mediaGrid())
        } else if (mode === "u-badge") {
            Qt.callLater(() => focusSection(mediaIndex(), 2, false))
        } else if (mode === "u-recordpanel") {
            testSource()
            recorder.folder = selfTestRecordDir
            iptv.group = "Alle zenders"
            iptvScreen.autoPlay = true
            iptvScreen.autoRecord = "panelonly"
            iptvScreen.open(mediaGrid())
        } else if (mode === "u-libsetup") {
            libraryScreen.open("movies", mediaGrid())
            libraryScreen.openSetup()
        }
    }

    function addTile(name, target) {
        // Een pad naar een bestand is een programma; al het andere een webadres
        const isFile = /^[a-zA-Z]:[\\/]/.test(target.trim()) || target.trim().startsWith("\\\\")
                       || target.trim().startsWith("\"") || target.trim().startsWith("file:")
        // Website: in de rij waar je het laatst was (Muziek, Podcasts, ...), anders bij Video
        const webTarget = currentSection && currentSection.model !== appModel ? currentSection.model : webVideo
        const ok = isFile ? appModel.addTile(name, target) : webTarget.addTile(name, target)
        if (!ok) {
            showToast(qsTr("Toevoegen mislukt: controleer het adres of pad (of de tegel bestaat al)"))
        } else {
            showToast(qsTr("Tegel toegevoegd"))
        }
    }

    Component.onCompleted: {
        if (selfTest.indexOf("tabs") === 0) layoutName = "tabs"
        focusSection(0, 0, false)
        startWhereLeft.start()
        // Eerste keer: welkomststap (diensten kiezen)
        if ((selfTest === "" && !appModel.welcomeDone) || selfTest === "welcome") {
            Qt.callLater(() => welcomeOverlay.open(mediaGrid()))
        }
        if (selfTest === "addtile") {
            addOverlay.open(quickRow)
        } else if (selfTest === "podcasts") {
            Qt.callLater(() => focusSection(sectionDefs.findIndex(d => d.model === webPodcasts), 1, false))
        } else if (selfTest === "moveask") {
            focusSection(0, 2, false)
            editModel = webVideo
            editRow = 2
            Qt.callLater(() => handleMenuAction("move-down"))
        } else if (selfTest === "library") {
            libraryScreen.open("shows", mediaGrid())
        } else if (selfTest === "librarymovies") {
            libraryScreen.open("movies", mediaGrid())
        } else if (selfTest === "librarydetail") {
            libraryScreen.autoOpen = "detail"
            libraryScreen.open("shows", mediaGrid())
        } else if (selfTest === "libraryepisodes") {
            libraryScreen.open("shows", mediaGrid())
            testStep.run(30000, () => {
                const shows = library.shows
                let withInfo = 0
                for (let i = 0; i < shows.length; ++i) {
                    const eps = shows[i].episodes || []
                    for (let j = 0; j < eps.length; ++j) {
                        if (eps[j].overview) { ++withInfo; break }
                    }
                }
                console.info("Episode info", withInfo >= 1 ? "ok" : "fail", withInfo, "of", shows.length)
            })
        } else if (selfTest === "iptv" || selfTest === "iptvplay" || selfTest === "iptvsources") {
            // Test: openbare lijst met vrij beschikbare Nederlandse zenders (iptv-org)
            if (iptv.sources.length === 0) {
                iptv.addSource("m3u-url", "Test", "https://iptv-org.github.io/iptv/countries/nl.m3u", "", "")
            }
            iptvScreen.autoPlay = selfTest === "iptvplay"
            iptvScreen.open(mediaGrid())
            if (selfTest === "iptvsources") {
                iptvScreen.openSources()
            }
        } else if (selfTest === "iptvbar" || selfTest === "iptvepg") {
            iptv.group = "Alle zenders"   // test: de testzenders staan niet in de groep Nederland
            // Test: eigen lijst met gids (aangemaakt door de CI)
            if (iptv.sources.length === 0) {
                iptv.addSource("m3u-file", "Gids-test", selfTestM3u, "", "")
            }
            iptvScreen.autoPlay = true
            iptvScreen.pinInfo = selfTest === "iptvbar"
            iptvScreen.autoGuide = selfTest === "iptvepg"
            iptvScreen.open(mediaGrid())
        } else if (selfTest.startsWith("frame")) {
            // Test: frame-chicago, frame-met, frame-nature, frame-mix, frame-lijst, frame-settings
            const kinds = { "frame-chicago": 0, "frame-met": 1, "frame-nature": 2, "frame-unsplash": 3, "frame-mix": 4, "frame-lijst": 0, "frame-settings": 2, "frame-esc": 2, "frame-mouseesc": 0 }
            frame.source = kinds[selfTest] !== undefined ? kinds[selfTest] : 0
            frame.fill = selfTest !== "frame-lijst"
            frame.interval = 5
            frameScreen.open(mediaGrid())
            if (selfTest === "frame-settings") {
                frameScreen.openSettings()
            }
        } else if (selfTest === "record" || selfTest === "recordpanel") {
            iptv.group = "Alle zenders"   // test: de testzenders staan niet in de groep Nederland
            // Test: zender uit de testlijst 20 seconden opnemen naar een testmap
            if (iptv.sources.length === 0) {
                iptv.addSource("m3u-file", "Gids-test", selfTestM3u, "", "")
            }
            recorder.folder = selfTestRecordDir
            iptvScreen.autoPlay = true
            iptvScreen.autoRecord = selfTest
            iptvScreen.open(mediaGrid())
        } else if (selfTest === "iptvfreeguide") {
            // Test: gratis zenders met de echte nachtelijke gids; NPO 1 met de balk vast in beeld
            iptv.addFreeSources()
            iptv.group = "Alle zenders"
            iptv.search = "NPO 1"
            iptvScreen.autoPlay = true
            iptvScreen.pinInfo = true
            iptvScreen.open(mediaGrid())
        } else if (selfTest.startsWith("u-")) {
            root.runImprovementShot(selfTest)
        } else if (selfTest === "iptvfree") {
            iptvScreen.open(mediaGrid())
        } else if (selfTest.startsWith("m-")) {
            // Schermafbeeldingen voor de handleiding
            if (iptv.sources.length === 0) {
                iptv.addSource("m3u-file", "Nederlandse zenders", selfTestM3u, "", "")
            }
            if (selfTest === "m-home") {
                const media = sectionDefs.findIndex(d => d.model === appModel)
                focusSection(media, 2, false)
                return
            }
            iptvScreen.demo = selfTest
            iptvScreen.autoPlay = selfTest === "m-bar" || selfTest.startsWith("m-guide") && selfTest !== "m-guidelist"
            iptvScreen.pinInfo = selfTest === "m-bar"
            iptvScreen.autoGuide = selfTest === "m-guide" || selfTest === "m-guide2"
            iptvScreen.guideStart = selfTest === "m-guide2" ? 3 : 0
            iptvScreen.open(mediaGrid())
        } else if (selfTest === "radio") {
            // Test: alle zeven zenders na elkaar laten spelen (log: "Radio playing")
            radioPlayer.open(0, mediaGrid())
            radioTestTimer.start()
        } else if (selfTest === "search") {
            Qt.callLater(() => root.openPicker("all", qsTr("Zoeken"), quickRow, "npo"))
            testStep.run(2500, () => { if (picker.results.length > 0) console.info("Search results", picker.results.length) })
        } else if (selfTest === "radioadd") {
            Qt.callLater(() => { root.openRadioSearch(); picker.text = "gelderland" })
            testStep.run(8000, () => { root.pickResult(0); console.info("Radio count", radio.count) })
        } else if (selfTest === "parental") {
            parental.setPin("2468")
            console.info("Parental check wrong", parental.check("1111"))
            console.info("Parental check right", parental.check("2468"))
            parental.lockNow()
            testStep.run(1500, () => {
                for (let r = 0; r < appModel.count; ++r) {
                    if (appModel.pathAt(r) === "bigscreen:iptv") root.launch(appModel, r)
                }
                pinPad.press("1"); pinPad.press("3")
            })
        } else if (selfTest === "sleep") {
            radioPlayer.open(0, mediaGrid())
            sleepTimer.setSeconds(10)
        } else if (selfTest === "weather") {
            weather.refresh()
            Qt.callLater(() => focusSection(0, 0, false))
        } else if (selfTest === "update") {
            updater.check(true)
        } else if (selfTest === "cec") {
            Qt.callLater(() => focusSection(0, 0, false))
            testStep.run(2000, () => {
                cec.parseLine("NOTICE:  [    1532]	connection opened")
                cec.parseLine("DEBUG:   [    2012]	key pressed: right (4) current(ff) duration(0)")
                cec.parseLine("DEBUG:   [    2112]	key released: right (4) D:100ms")
                cec.parseLine("DEBUG:   [    2312]	key pressed: right (4) current(ff) duration(0)")
                cec.parseLine("DEBUG:   [    2412]	key released: right (4) D:100ms")
            })
        } else if (selfTest === "cleanup") {
            recorder.folder = selfTestRecordDir
            recorder.keepDays = 30
            testStep.run(1500, () => recorder.checkCleanup(true))
        } else if (selfTest === "radiobg") {
            radioPlayer.open(0, mediaGrid())
            testStep.run(5000, () => {
                radioPlayer.close()
                Qt.callLater(() => focusSection(sectionDefs.findIndex(d => d.model === appModel), 0, false))
            })
        } else if (selfTest === "radioprogramme") {
            radioPlayer.open(Math.max(0, radio.rowOfName("NPO Radio 2")), mediaGrid())
        } else if (selfTest === "nlchannels") {
            iptvScreen.open(mediaGrid(), "NPO & RTL")
            testStep.run(30000, () => {
                let withGuide = 0
                for (let r = 0; r < iptv.count; ++r) if (iptv.nowNext(r).nowTitle) ++withGuide
                console.info("NL guide", withGuide >= 8 ? "ok" : "fail", withGuide, "of", iptv.count)
                console.info("NL web channels", iptv.isWebChannel(0))
            })
        } else if (selfTest === "radiodrift") {
            radioPlayer.idleMs = 2000
            radioPlayer.open(0, mediaGrid())
                } else if (selfTest === "regional") {
            // Tegel Regionale tv: lijst, daarna eerste zender afspelen
            testStep.run(1500, () => {
                for (let r = 0; r < appModel.count; ++r) if (appModel.pathAt(r) === "bigscreen:regional") root.launch(appModel, r)
                console.info("Regional count", iptv.count, iptv.group)
                testStep.run(2500, () => iptvScreen.play(3))
            })
        } else if (selfTest === "nowplaying") {
            radioPlayer.open(0, mediaGrid())
            testStep.run(4000, () => {
                radioPlayer.close()
                Qt.callLater(() => {
                    focusSection(sectionDefs.findIndex(d => d.model === appModel), 0, false)
                    console.info("Media first tile:", appModel.nameAt(0))
                })
            })
        } else if (selfTest === "sections") {
            appModel.sectionOrder = []
            console.info("Section order default:", sectionDefs.map(d => d.title).join(","))
            moveSection(radio, -1)
            console.info("Section order moved:", sectionDefs.map(d => d.title).join(","))
            radio.move(radio.count - 1, 0)
            console.info("Radio first tile:", radio.nameAt(0), radio.count)
            Qt.callLater(() => focusSection(sectionDefs.findIndex(d => d.model === radio), 0, false))
        } else if (selfTest === "homekey") {
            homeBridge.externalLaunched()
            testStep.run(1500, () => homeBridge.goHome())
        } else if (selfTest === "releaseinfo") {
            Qt.callLater(() => releaseInfo.open(quickRow))
        } else if (selfTest === "news") {
            Qt.callLater(() => focusSection(sectionDefs.findIndex(d => d.model === webNews), 0, false))
        } else if (selfTest === "photo") {
            Qt.callLater(() => root.openPicker("photo", qsTr("Gratis foto als achtergrond"), quickRow, "bos"))
        } else if (selfTest === "keys") {
            Qt.callLater(() => {
                keysOverlay.open(quickRow)
                apiKeys.setUnsplashKey("test-only")
                console.info("Keys overlay opened", keysOverlay.visible, "set", apiKeys.unsplashSet)
                apiKeys.setUnsplashKey("")
            })
        } else if (selfTest === "settingsgroups") {
            const groups = ["home", "look", "family", "devices", "tv", "system"]
            let n = 0
            const next = () => {
                if (n >= groups.length) return
                root.openSettings(groups[n++])
                testStep.run(1200, next)
            }
            Qt.callLater(next)
        } else if (selfTest === "settingsmenu") {
            Qt.callLater(() => root.openPersonalizeMenu())
        } else if (selfTest === "usage") {
            // Een paar keer iets openen: rij Vaak gebruikt verschijnt bovenaan
            usage.record("video", webVideo.pathAt(0)); usage.record("video", webVideo.pathAt(0))
            usage.record("video", webVideo.pathAt(1)); usage.record("radio", radio.pathAt(0))
            usage.record("media", appModel.pathAt(0))
            console.info("Usage count", usage.count, usage.nameAt(0))
            Qt.callLater(() => focusSection(0, 0, false))
        } else if (selfTest === "help") {
            Qt.callLater(() => helpOverlay.open(quickRow))
        } else if (selfTest === "night") {
            Qt.callLater(() => focusSection(0, 0, false))
        } else if (selfTest === "profiles") {
            const pid = profiles.add("Petra", false)
            console.info("Profiles count", profiles.count, profiles.nameOf(pid))
            Qt.callLater(() => openProfileMenu())
        } else if (selfTest === "security") {
            // Inloggegevens mogen niet in het log komen (de test zoekt "geheimwachtwoord" in het logbestand)
            console.warn("Security test url http://voorbeeld.tv:8080/kees/geheimwachtwoord/1332 en get.php?username=kees&password=geheimwachtwoord")
            if (iptv.sources.length === 0) iptv.addSource("m3u-file", "Gids-test", selfTestM3u, "", "")
            // Raden van de code: na 5 foute pogingen geblokkeerd
            remote.enabled = true
            let n = 0
            const next = () => {
                const x = new XMLHttpRequest()
                const right = n >= 6
                x.open("POST", remote.address + "/k?c=" + (right ? remote.code : "99999" + n))
                x.onreadystatechange = () => {
                    if (x.readyState !== XMLHttpRequest.DONE) return
                    if (right) { console.info("Remote test blocked", x.status); return }
                    n++
                    next()
                }
                x.send("down")
            }
            testStep.run(1500, next)
        } else if (selfTest === "english") {
            console.info("English check:", qsTr("Instellingen"), "/", qsTr("Nu op tv"), "/", appModel.nameAt(0))
            Qt.callLater(() => openMoreMenu())
        } else if (selfTest === "libraryesc") {
            // Test: Films & Series openen; de test stuurt een echte Esc (terug naar het startscherm)
            libraryScreen.open("movies", mediaGrid())
        } else if (selfTest === "okkey") {
            openPicker("name", qsTr("Naam van het nieuwe profiel"), quickRow, qsTr("Kees"))
            testStep.run(2500, () => picker.pressOk())
        } else if (selfTest === "nowtv" || selfTest === "guidegrid" || selfTest === "reminder" || selfTest === "series" || selfTest === "prevchannel") {
            iptv.group = "Alle zenders"
            if (iptv.sources.length === 0) iptv.addSource("m3u-file", "Gids-test", selfTestM3u, "", "")
            if (selfTest === "nowtv") {
                testStep.run(4000, () => {
                    for (let r = 0; r < 3; ++r) if (!iptv.isFavorite(r)) iptv.toggleFavorite(r)
                    nowTv.refresh()
                    console.info("Now tv count ok", nowTv.count >= 3, nowTv.count, nowTv.subtitleAt(0))
                    Qt.callLater(() => focusSection(sectionDefs.findIndex(d => d.model === nowTv), 0, false))
                })
            } else if (selfTest === "guidegrid") {
                iptvScreen.open(mediaGrid())
                testStep.run(5000, () => iptvScreen.openGrid())
            } else if (selfTest === "reminder") {
                testStep.run(4000, () => {
                    const ch = iptv.channelAt(0)
                    iptv.toggleReminder(ch.name, ch.url, ch.logo || "", "Testprogramma", Date.now() / 1000 + 60)
                })
            } else if (selfTest === "series") {
                iptvScreen.autoPlay = true
                iptvScreen.open(mediaGrid())
                testStep.run(7000, () => {
                    const g = iptv.guide(0)
                    const ch = iptv.channelAt(0)
                    console.info("Series test: guide", g.length)
                    if (g.length > 1) iptv.toggleSeries(ch.name, ch.url, g[1].title)
                    iptvScreen.openGuide(0)
                })
            } else {
                iptvScreen.autoPlay = true
                iptvScreen.open(mediaGrid())
                testStep.run(5000, () => {
                    iptvScreen.play(1)
                    testStep.run(3000, () => iptvScreen.previousChannel())
                })
            }
        } else if (selfTest.indexOf("tabs") === 0) {
            // Test: tabbladen-layout met favoriete zenders (Nu op tv) en Verder kijken
            iptv.group = "Alle zenders"
            if (iptv.sources.length === 0) iptv.addSource("m3u-file", "Gids-test", selfTestM3u, "", "")
            testStep.run(4000, () => {
                for (let r = 0; r < 3; ++r) if (!iptv.isFavorite(r)) iptv.toggleFavorite(r)
                nowTv.refresh()
                resume.refresh()
                Qt.callLater(() => root.tabsTest(selfTest))
            })
        } else if (selfTest === "ambient") {
            Qt.callLater(() => focusSection(0, 0, false))
        } else if (selfTest === "audio") {
            Qt.callLater(() => openAudioMenu(quickRow))
        } else if (selfTest === "remote") {
            remote.enabled = true
            remoteOverlay.open(quickRow)
            testStep.run(2000, () => {
                // Zoals de telefoon: eerst een verkeerde code, dan de goede
                const send = (code, key) => {
                    const x = new XMLHttpRequest()
                    x.open("POST", remote.address + "/k?c=" + code)
                    x.onreadystatechange = () => { if (x.readyState === XMLHttpRequest.DONE) console.info("Remote test", key, code === remote.code ? "right code" : "wrong code", x.status) }
                    x.send(key)
                }
                send("0000", "up")
                send(remote.code, "down")
                // Als app: manifest en pictogram
                const m = new XMLHttpRequest()
                m.open("GET", remote.address + "/manifest.webmanifest?c=" + remote.code)
                m.onreadystatechange = () => { if (m.readyState === XMLHttpRequest.DONE) {
                    let ok = false
                    try { const j = JSON.parse(m.responseText); ok = j.display === "standalone" && j.icons.length === 3 && j.start_url.indexOf(remote.code) > 0 } catch (e) {}
                    console.info("Remote manifest", m.status, ok ? "standalone" : "bad")
                } }
                m.send()
                const ic = new XMLHttpRequest()
                ic.open("GET", remote.address + "/icon-192.png")
                ic.onreadystatechange = () => { if (ic.readyState === XMLHttpRequest.DONE) console.info("Remote icon", ic.status, ic.getResponseHeader("Content-Length")) }
                ic.send()
            })
        } else if (selfTest === "whoswatching") {
            // startWhereLeft vraagt het (er zijn 2+ profielen uit eerdere tests)
        } else if (selfTest === "kidslimit") {
            parental.setKidsMinutes(30)
            parental.setKidsUsed(kidsTimer.today(), 40)
            testStep.run(2000, () => { kidsLock.open(); console.info("Kids lock shown", kidsLock.visible) })
        } else if (selfTest === "backup") {
            console.info("Backup made:", systemTools.makeBackup(), "log:", systemTools.shareLog(), "autostart", systemTools.autostart)
            systemTools.autostart = true
            console.info("Autostart on:", systemTools.autostart)
            systemTools.autostart = false
            console.info("Autostart off:", systemTools.autostart)
            Qt.callLater(() => { menu.open(qsTr("Back-up"), [{ label: qsTr("Back-up maken"), action: "" }].concat(systemTools.backups().map(f => ({ label: qsTr("Terugzetten: ") + f, action: "" }))), quickRow) })
        } else if (selfTest === "collapse") {
            appModel.setSetting("collapsed", ["Podcasts"])
            console.info("Collapsed Podcasts:", isCollapsed("Podcasts", webPodcasts))
            Qt.callLater(() => focusSection(0, 0, false))
        } else if (selfTest === "radiohome") {
            Qt.callLater(() => focusSection(sectionDefs.findIndex(d => d.model === radio), 0, false))
        } else if (selfTest === "resume") {
            // Test: tegel Verder kijken in de rij Media openen (geschiedenis door de test klaargezet)
            testStep.run(2000, () => {
                for (let r = 0; r < appModel.count; ++r) if (appModel.pathAt(r) === "bigscreen:resume") root.launch(appModel, r)
            })
        } else if (selfTest === "epgrecord") {
            iptv.group = "Alle zenders"   // test: de testzenders staan niet in de groep Nederland
            // Test: in de gids het programma van nu en een later programma laten opnemen
            if (iptv.sources.length === 0) {
                iptv.addSource("m3u-file", "Gids-test", selfTestM3u, "", "")
            }
            recorder.folder = selfTestRecordDir
            iptvScreen.autoPlay = true
            iptvScreen.autoGuide = true
            iptvScreen.autoGuideRecord = true
            iptvScreen.guideStart = 3
            iptvScreen.open(mediaGrid())
        } else if (selfTest === "mpcplay") {
            // Test: eerste serie openen en vanaf de eerste aflevering afspelen in MPC-HC
            libraryScreen.autoOpen = "play"
            libraryScreen.open("shows", mediaGrid())
        } else if (selfTest === "librarysetup") {
            libraryScreen.open("movies", mediaGrid())
            libraryScreen.openSetup()
        }
    }

    // Eigen achtergrond, met een donkere laag erover zodat tekst leesbaar blijft
    Image {
        anchors.fill: parent
        visible: appModel.background !== ""
        source: appModel.background
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: false
        // Links en onder donkerder, zodat tekst en tegels leesbaar blijven
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#d00b0c0f" }
                GradientStop { position: 0.6; color: "#500b0c0f" }
                GradientStop { position: 1.0; color: "#200b0c0f" }
            }
        }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#000b0c0f" }
                GradientStop { position: 0.45; color: "#600b0c0f" }
                GradientStop { position: 1.0; color: "#e80b0c0f" }
            }
        }
    }

    ListModel {
        id: quickModel
        ListElement { label: qsTr("Zoeken"); code: 0xE721; uri: "search"; about: qsTr("Zoek apps, zenders, programma's, films en radio") }
        ListElement { label: qsTr("Hulp"); code: 0xE897; uri: "help"; about: qsTr("Welke knop doet wat?") }
        ListElement { label: qsTr("Geluid"); code: 0xE767; uri: "audio"; about: qsTr("Tv, soundbar of koptelefoon") }
        ListElement { label: qsTr("Instellingen"); code: 0xE713; uri: "personalize"; about: qsTr("Tegels, achtergrond, kinderslot en meer") }
        ListElement { label: qsTr("Sluiten"); code: 0xE711; uri: ""; about: qsTr("Kijkluister sluiten (de computer blijft aan)") }
    }

    // --- Bovenbalk: naam links, snelknoppen in het midden, klok rechts ---
    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: root.unit * 3
        height: root.unit * 6

        Image {
            id: brandIcon
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: root.unit * 4.4
            height: width
            source: "qrc:/assets/kijkluister-256.png"
            sourceSize: Qt.size(128, 128)
            smooth: true
        }
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            id: brand
            anchors.left: brandIcon.right
            anchors.leftMargin: root.unit * 1.2
            anchors.verticalCenter: parent.verticalCenter
            color: "white"
            font.pixelSize: (root.unit * 3.2) * appModel.fontScale
            font.weight: Font.DemiBold
            text: qsTr("Kijkluister")
        }

        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            id: clock
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            color: "white"
            font.pixelSize: (root.unit * 3.2) * appModel.fontScale
        }
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            id: dateLabel
            anchors.right: clock.left
            anchors.rightMargin: root.unit * 2
            anchors.baseline: clock.baseline
            color: "#c4c7cc"
            font.pixelSize: (root.unit * 2.4) * appModel.fontScale
        }
        // Weer (Open-Meteo) en de slaaptimer naast de datum
        Row {
            id: statusRow
            anchors.right: dateLabel.left
            anchors.rightMargin: root.unit * 2
            anchors.verticalCenter: parent.verticalCenter
            spacing: root.unit * 1.6
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                visible: sleepTimer.active
                anchors.verticalCenter: parent.verticalCenter
                text: "🌙 " + sleepTimer.remainingText
                color: "#c4c7cc"
                font.pixelSize: (root.unit * 2.4) * appModel.fontScale
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                id: weatherLabel
                visible: weather.available
                anchors.verticalCenter: parent.verticalCenter
                text: weather.icon + " " + weather.summary
                color: "#c4c7cc"
                font.pixelSize: (root.unit * 2.4) * appModel.fontScale
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.handleMenuAction("weather")
                }
            }
        }
    }

    Timer {
        interval: 1000; running: true; repeat: true; triggeredOnStart: true
        onTriggered: {
            const now = new Date()
            clock.text = Qt.formatTime(now, qsTr("HH:mm"))
            dateLabel.text = now.toLocaleDateString(Qt.locale(qsTr("nl_NL")), qsTr("dddd d MMMM"))
        }
    }

    // --- Snelknoppen in de bovenbalk ---
    ListView {
        id: quickRow
        z: 5   // labeltje onder een knop valt over het tekstvlak eronder
        anchors.verticalCenter: header.verticalCenter
        anchors.left: header.left
        anchors.leftMargin: brand.x + brand.width + root.unit * 3
        width: Math.min(root.unit * 19 * quickModel.count,
                        header.width - brand.x - brand.width - clock.width - dateLabel.width - statusRow.width - root.unit * 12)
        height: root.unit * 5.6
        orientation: ListView.Horizontal
        model: quickModel
        keyNavigationEnabled: true
        interactive: false

        function trigger(index) {
            const item = quickModel.get(index)
            if (item.uri === "") {
                // Eerst vragen: de computer blijft aan, geplande opnames gaan niet door
                menu.open(qsTr("Kijkluister sluiten?"), [
                    { label: qsTr("Sluiten"), action: "quit" },
                    { label: qsTr("Annuleren"), action: "" }
                ], root.menuFocus(), recorder.schedule.length > 0
                    ? qsTr("Je computer blijft aan. Geplande opnames gaan niet door zolang Kijkluister dicht is.")
                    : qsTr("Je computer blijft aan."))
            } else if (item.uri === "personalize") {
                root.openPersonalizeMenu()
            } else if (item.uri === "help") {
                helpOverlay.open(quickRow)
            } else if (item.uri === "audio") {
                root.openAudioMenu(quickRow)
            } else if (item.uri === "profile") {
                root.openProfileMenu()
            } else if (item.uri === "sleep") {
                root.openSleepMenu(quickRow)
            } else if (item.uri === "search") {
                root.openPicker("all", qsTr("Zoeken"), root.menuFocus())
            } else if (item.uri === "sleep") {
                root.openSleepMenu(quickRow)
            } else if (item.uri === "radio") {
                radioPlayer.show(quickRow)
            } else {
                appModel.openUri(item.uri)
                root.showToast(item.label + qsTr(" openen…"))
            }
        }

        delegate: QuickTile {
            required property int code
            glyph: String.fromCharCode(code)
            width: quickRow.width / quickModel.count
            height: quickRow.height
            compact: width < root.unit * 16
            selected: quickRow.activeFocus && quickRow.currentIndex === index
            onActivated: { quickRow.currentIndex = index; quickRow.trigger(index) }
            onHovered: { if (!menu.visible && !addOverlay.visible && root.mouseActive()) { quickRow.currentIndex = index; quickRow.forceActiveFocus() } }
        }

        Keys.onReturnPressed: trigger(currentIndex)
        Keys.onEnterPressed: trigger(currentIndex)
        Keys.onDownPressed: { if (root.tabsMode) tabList.forceActiveFocus(); else root.focusSection(0, 0, false) }
    }

    // --- Tabbladen (alleen in de tabbladen-layout) ---
    Item {
        id: tabBar
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.unit * 4
        anchors.rightMargin: root.unit * 4
        height: root.tabsMode ? root.unit * 6.5 : 0
        visible: root.tabsMode

        ListView {
            id: tabList
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(parent.width, contentWidth)
            height: parent.height
            orientation: ListView.Horizontal
            spacing: root.unit * 1.2
            interactive: false
            keyNavigationEnabled: false
            model: root.visibleTabs
            currentIndex: root.tab
            Keys.onLeftPressed: root.selectTab(root.tab - 1)
            Keys.onRightPressed: root.selectTab(root.tab + 1)
            Keys.onUpPressed: quickRow.forceActiveFocus()
            Keys.onDownPressed: root.enterTabContent()
            Keys.onReturnPressed: root.enterTabContent()
            Keys.onEnterPressed: root.enterTabContent()

            delegate: Item {
                id: tabItem
                required property int index
                required property var modelData
                readonly property bool current: index === root.tab
                width: tabLabel.implicitWidth + root.unit * 4
                height: tabList.height
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: root.unit * 0.5
                    radius: height / 2
                    color: tabList.activeFocus && tabItem.current ? "#40ffffff" : "transparent"
                }
                Text {
                    id: tabLabel
                    textFormat: Text.PlainText
                    anchors.centerIn: parent
                    color: tabItem.current ? "white" : "#aab0b8"
                    font.pixelSize: (root.unit * 2.7) * appModel.fontScale
                    font.weight: tabItem.current ? Font.DemiBold : Font.Normal
                    text: tabItem.modelData.label
                }
                Rectangle {
                    visible: tabItem.current
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: root.unit * 0.3
                    width: parent.width - root.unit * 3
                    height: Math.max(3, root.unit * 0.35)
                    radius: height / 2
                    color: "white"
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: { tabList.forceActiveFocus(); root.selectTab(tabItem.index) }
                    onEntered: { if (root.mouseActive() && !menu.visible) { tabList.forceActiveFocus(); root.selectTab(tabItem.index) } }
                }
            }
        }
    }

    // --- Groot vlak boven de rijen in de tabbladen-layout: wat heeft focus (of de eerste tegel) ---
    Item {
        id: tabsHero
        visible: root.tabsMode
        anchors.top: tabBar.bottom
        anchors.topMargin: root.tabsMode ? root.unit * 1.5 : 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.unit * 4
        anchors.rightMargin: root.unit * 4
        height: root.tabsMode ? root.unit * 22 : 0
        clip: true

        readonly property var heroModel: root.focusModel ? root.focusModel : (root.sectionDefs.length > 0 ? root.sectionDefs[0].model : null)
        readonly property int heroRow: root.focusModel ? root.focusRow : (heroModel && heroModel.count > 0 ? 0 : -1)
        readonly property string heroName: heroModel && heroRow >= 0 ? (heroModel.revision, heroModel.nameAt(heroRow)) : ""
        readonly property string heroKind: root.focusModel ? root.focusKind : (root.sectionDefs.length > 0 ? (root.sectionDefs[0].label || root.sectionDefs[0].title) : "")
        readonly property string heroSub: heroModel && heroRow >= 0 && heroModel.subtitleAt ? (heroModel.revision, heroModel.subtitleAt(heroRow)) : ""
        readonly property string ownBackdrop: heroModel && heroRow >= 0 && heroModel.backdropAt ? (heroModel.revision, heroModel.backdropAt(heroRow)) : ""
        // Geen eigen plaatje: bij Voor jou het plaatje van het laatste wat je keek
        readonly property string heroBackdrop: ownBackdrop !== "" ? ownBackdrop
                                              : (root.tabId() === "foryou" && resume.count > 0 ? resume.backdropAt(0) : "")
        readonly property bool tvTab: root.tabId() === "tv"

        // Plaatje rechts
        Rectangle {
            id: heroCard
            visible: heroImage.status === Image.Ready
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height - root.unit * 2
            width: height * 1.7778
            radius: root.unit * 1
            color: "#22000000"
            clip: true
            Image {
                id: heroImage
                anchors.fill: parent
                source: tabsHero.heroBackdrop
                sourceSize.width: 960
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
            }
        }

        Column {
            anchors.left: parent.left
            anchors.leftMargin: root.unit * 1.5
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width * (heroCard.visible ? 0.5 : 0.8)
            spacing: root.unit * 1.2

            Text {
                textFormat: Text.PlainText
                width: parent.width
                color: "#c4c7cc"
                font.pixelSize: (root.unit * 2.2) * appModel.fontScale
                elide: Text.ElideRight
                text: tabsHero.tvTab ? qsTr("TV & Gids") : tabsHero.heroKind
            }
            Text {
                textFormat: Text.PlainText
                width: parent.width
                color: "white"
                font.pixelSize: (root.unit * 6.2) * appModel.fontScale
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                text: tabsHero.tvTab ? qsTr("Kijk live of bekijk de gids") : tabsHero.heroName
            }
            Text {
                textFormat: Text.PlainText
                width: parent.width
                visible: text !== "" && !tabsHero.tvTab
                color: "#d5d8dc"
                font.pixelSize: (root.unit * 2.4) * appModel.fontScale
                elide: Text.ElideRight
                text: tabsHero.heroSub
            }
            // Op elk tabblad behalve TV & Gids: aanwijzing dat Enter de tegel opent
            Rectangle {
                visible: !tabsHero.tvTab && tabsHero.heroName !== ""
                width: openLabel.implicitWidth + root.unit * 5
                height: root.unit * 5.4
                radius: height / 2
                color: "#3d6fb4"
                Text {
                    id: openLabel
                    textFormat: Text.PlainText
                    anchors.centerIn: parent
                    color: "white"
                    font.pixelSize: (root.unit * 2.4) * appModel.fontScale
                    font.weight: Font.DemiBold
                    text: qsTr("Openen")
                }
            }
            // Tabblad TV & Gids: live tv en de gids
            Row {
                visible: tabsHero.tvTab
                spacing: root.unit * 1.5
                FocusButton {
                    id: liveButton
                    unit: root.unit
                    text: qsTr("Live tv kijken")
                    onClicked: root.openLiveTv()
                    KeyNavigation.right: guideButton
                    Keys.onUpPressed: tabList.forceActiveFocus()
                    Keys.onDownPressed: root.focusSection(0, 0, false)
                }
                FocusButton {
                    id: guideButton
                    unit: root.unit
                    text: qsTr("Tv-gids")
                    onClicked: root.openTvGuide()
                    KeyNavigation.left: liveButton
                    Keys.onUpPressed: tabList.forceActiveFocus()
                    Keys.onDownPressed: root.focusSection(0, 0, false)
                }
            }
        }
    }

    // --- Groot tekstvlak: wat heeft nu focus ---
    Item {
        id: hero
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.unit * 4
        anchors.rightMargin: root.unit * 4
        // Ingeklapt als er (nog) niets te tonen is, zodat er geen leeg vlak boven de rijen staat
        readonly property bool filled: !root.tabsMode && root.heroShown && root.focusName !== ""
        height: filled ? root.unit * 21 : root.unit * 1
        opacity: filled ? 1 : 0
        clip: true
        Behavior on height { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 180 } }

        Image {
            id: heroIcon
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.bottomMargin: root.unit * 2.5
            width: root.focusIcon !== "" ? root.unit * 13 : 0
            height: root.unit * 13
            sourceSize: Qt.size(256, 256)
            source: root.focusIcon
            fillMode: Image.PreserveAspectFit
            asynchronous: true
        }

        Column {
            anchors.left: heroIcon.right
            anchors.leftMargin: heroIcon.width > 0 ? root.unit * 3 : 0
            anchors.bottom: parent.bottom
            anchors.bottomMargin: root.unit * 2.5
            width: parent.width * 0.7
            spacing: root.unit

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                color: "white"
                font.pixelSize: (root.unit * 7.5) * appModel.fontScale
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                text: root.focusName
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                color: "#c4c7cc"
                font.pixelSize: (root.unit * 2.4) * appModel.fontScale
                text: root.focusKind
            }
        }
    }

    // --- Rijen onder elkaar: Video, Muziek, Podcasts, Media (scrollt) ---
    Flickable {
        id: page
        anchors.top: root.tabsMode ? tabsHero.bottom : hero.bottom
        anchors.bottom: hint.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: root.unit
        anchors.leftMargin: root.unit * 1.5
        anchors.rightMargin: root.unit * 1.5
        contentWidth: width
        contentHeight: sectionColumn.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // Na een maatwijziging (start, ander scherm) de gekozen tegel weer in beeld brengen
        onHeightChanged: if (root.currentSection) root.scrollTo(root.currentSection)
        onContentHeightChanged: if (root.currentSection) root.scrollTo(root.currentSection)
        Behavior on contentY { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        Column {
            id: sectionColumn
            width: page.width
            spacing: root.unit * 0.5

            Repeater {
                id: sections
                model: root.sectionDefs
                delegate: SectionGrid {
                    id: sectionItem
                    required property int index
                    required property var modelData
                    width: sectionColumn.width
                    title: modelData.label || modelData.title
                    model: modelData.model
                    collapsed: !root.tabsMode && root.isCollapsed(modelData.title, modelData.model)
                    strip: root.tabsMode
                    columns: root.columns
                    unit: root.unit
                    canHover: () => !menu.visible && !addOverlay.visible && !placesPicker.visible && !picker.visible && !pinPad.visible && root.mouseActive()
                    onExitUp: (column) => root.focusSection(index - 1, column, true)
                    onExitDown: (column) => root.focusSection(index + 1, column, false)
                    onLaunchRequested: (row) => root.launch(sectionItem.model, row)
                    onMenuRequested: (row) => root.openTileMenu(sectionItem.model, row, sectionItem.grid)
                    onTileFocused: {
                        root.currentSection = sectionItem
                        root.scrollTo(sectionItem)
                    }
                }
            }
        }
    }

    HintBar {
        id: hint
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: root.unit * 1.5
        unit: root.unit
        items: [
            { pad: "✛", key: qsTr("Pijltjes"), label: qsTr("Kiezen") },
            { pad: "A", key: qsTr("Enter"), label: qsTr("Openen") },
            { pad: "X", key: qsTr("Menu"), label: qsTr("Opties") },
            { pad: "", key: qsTr("Rechtermuisknop"), label: qsTr("Opties") }
        ]
    }

    // --- Korte melding ---
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.unit * 8
        width: toast.implicitWidth + root.unit * 6
        height: root.unit * 7
        radius: height / 2
        color: "#e6000000"
        opacity: toastTimer.running ? 1 : 0
        visible: opacity > 0
        z: 200
        Behavior on opacity { NumberAnimation { duration: 200 } }

        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            id: toast
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: (root.unit * 2.6) * appModel.fontScale
        }
    }
    Timer { id: toastTimer; interval: 3500 }

    // Eenmalige tip, de eerste keer op het startscherm
    Timer {
        id: tipTimer
        interval: 2500
        running: selfTest === "" && appModel.welcomeDone && !appModel.tipShown("hold")
        onTriggered: {
            appModel.markTipShown("hold")
            root.showToast(qsTr("Tip: houd ") + (gamepad.connectedCount > 0 ? "Ⓐ" : "Enter") + qsTr(" ingedrukt om een tegel te verplaatsen of te verwijderen"), 7000)
        }
    }

    // --- Dialogen ---
    FileDialog {
        id: imageDialog
        property string purpose
        nameFilters: [qsTr("Afbeeldingen (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.ico *.svg)")]
        onAccepted: {
            const ok = purpose === "icon" ? root.editModel.setCustomIcon(root.editRow, selectedFile)
                                          : appModel.setBackground(selectedFile)
            if (!ok) {
                root.showToast(qsTr("Deze afbeelding kon niet gebruikt worden"))
            }
        }
    }

    FileDialog {
        id: programDialog
        title: qsTr("Programma kiezen")
        nameFilters: [qsTr("Programma's en snelkoppelingen (*.exe *.lnk *.bat *.cmd *.url)"), qsTr("Alle bestanden (*)")]
        onAccepted: addOverlay.targetText = selectedFile.toString()
    }

    PlacesPicker {
        id: placesPicker
        property var model: null
        property int row: -1
        property string direction: ""
        onAccepted: (places) => root.moveTile(model, row, direction, places)
    }

    MenuOverlay {
        id: menu
        onChosen: (action) => root.handleMenuAction(action)
    }

    KeysOverlay {
        id: keysOverlay
    }

    AddTileOverlay {
        id: addOverlay
        onAccepted: (name, target) => root.addTile(name, target)
        onBrowseRequested: programDialog.open()
    }

    // Live tv: zenderlijst en speler
    IptvScreen {
        onPinRequired: (row) => pinPad.ask(iptv.channelAt(row).name + qsTr(" is vergrendeld"), qsTr("Zender voor volwassenen. Vul de pincode van het kinderslot in."),
                                           (pin) => { if (!parental.check(pin)) return false; iptvScreen.play(row); return true }, iptvScreen)
        id: iptvScreen
    }

    // Schilderij: kunst of natuurfoto's
    FrameScreen {
        id: frameScreen
    }

    // Welkomststap: welke diensten gebruik je
    WelcomeOverlay {
        id: welcomeOverlay
        onDone: root.showToast(qsTr("Klaar! Kies een tegel en druk op ") + (gamepad.connectedCount > 0 ? "Ⓐ" : "Enter") + qsTr(" om te beginnen"))
    }

    // Melding als Kijkluister MPC-HC of ffmpeg ophaalt (op de achtergrond): bovenaan, uit de weg
    Rectangle {
        z: 950
        visible: components.status !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: root.height * 0.02
        width: toastRow.implicitWidth + root.height * 0.05
        height: root.height * 0.06
        radius: height / 2
        color: "#e6202124"
        border.color: "#33ffffff"
        Row {
            id: toastRow
            anchors.centerIn: parent
            spacing: root.height * 0.015
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.verticalCenter: parent.verticalCenter
                color: "white"
                font.pixelSize: (root.height * 0.024) * appModel.fontScale
                text: components.status
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                visible: components.progress >= 0
                width: root.height * 0.18
                height: root.height * 0.008
                radius: height / 2
                color: "#40ffffff"
                Rectangle {
                    width: parent.width * Math.max(0, components.progress) / 100
                    height: parent.height
                    radius: height / 2
                    color: "#ffb74d"
                }
            }
        }
    }

    // Radio: zenders luisteren in Kijkluister zelf
    RadioPlayer {
        id: radioPlayer
        sleepText: sleepTimer.active ? sleepTimer.remainingText : ""
        onSleepRequested: root.openSleepMenu(radioPlayer)
        // Speelt hij op de achtergrond: knop qsTr("Radio") in de bovenbalk
        onInBackgroundChanged: root.syncRadioButton()
        onRowChanged: root.syncRadioButton()
    }

    // Speelt de radio op de achtergrond: tegel "Nu op de radio" vooraan in de rij Media
    function syncRadioButton() {
        if (radioPlayer.inBackground) {
            const icon = radio.iconAt(radioPlayer.row)
            appModel.setNowPlaying(radio.nameAt(radioPlayer.row), icon.startsWith("glyph:") ? "" : icon, radio.accentAt(radioPlayer.row))
        } else {
            appModel.setNowPlaying("", "", "")
        }
    }

    Connections {
        target: radio
        function onAddRequested() { root.openRadioSearch() }
    }

    // --- Slaaptimer: na de gekozen tijd stopt alles wat speelt ---
    Item {
        id: sleepTimer
        property double endAt: 0
        property double now: Date.now()
        readonly property bool active: endAt > 0
        readonly property int remainingMinutes: active ? Math.max(0, Math.ceil((endAt - now) / 60000)) : 0
        readonly property string remainingText: remainingMinutes + qsTr(" min")

        function set(minutes) {
            setSeconds(minutes * 60)
            root.showToast(minutes > 0 ? qsTr("Slaaptimer: over ") + minutes + qsTr(" minuten stopt alles") : qsTr("Slaaptimer staat uit"))
        }
        function setSeconds(seconds) {
            endAt = seconds > 0 ? Date.now() + seconds * 1000 : 0
            now = Date.now()
            warned = false
            console.info("Sleep timer", seconds, "s")
        }
        property bool warned: false

        Timer {
            interval: 1000
            repeat: true
            running: sleepTimer.active
            onTriggered: {
                sleepTimer.now = Date.now()
                const left = sleepTimer.endAt - sleepTimer.now
                if (!sleepTimer.warned && left < 60000 && left > 5000) {
                    sleepTimer.warned = true
                    root.showToast(qsTr("Slaaptimer: over een minuut stopt alles"), 8000)
                }
                if (left <= 0) {
                    sleepTimer.endAt = 0
                    root.sleepNow()
                }
            }
        }
    }

    function sleepNow() {
        console.info("Sleep timer fired")
        radioPlayer.stopPlayback()
        if (iptvScreen.visible) iptvScreen.close()
        mpc.stop()
        showToast(qsTr("Slaaptimer: alles is gestopt. Welterusten!"), 10000)
    }

    // --- Zoeken (alles, radiozenders, plaats voor het weer) ---
    property string pickerMode: ""
    property var pickerItems: []

    function openPicker(mode, title, focusItem, startText) {
        pickerMode = mode
        picker.iconScale = mode === "photo" ? 2 : 1
        picker.voice = mode === "all" || mode === "radio" || mode === "photo"
        picker.placeholder = mode === "radio" ? qsTr("Naam van de zender, bv. Omroep Gelderland")
                           : mode === "photo" ? qsTr("Waar wil je een foto van? Bv. bos, zee, bergen")
                           : mode === "place" ? qsTr("Naam van je woonplaats")
                           : qsTr("Zoek een app, zender, programma, film of radiozender")
        picker.pickLabel = mode === "radio" ? qsTr("Toevoegen") : mode === "place" || mode === "photo" ? qsTr("Kiezen") : qsTr("Openen")
        picker.emptyText = mode === "radio" ? qsTr("Geen zender gevonden. Probeer een ander woord.") : qsTr("Niets gevonden")
        picker.open(title, focusItem, startText)
    }

    function openRadioSearch() {
        const section = sectionFor(radio)
        openPicker("radio", qsTr("Radiozender toevoegen"), section ? section.grid : quickRow)
    }

    function searchAll(q) {
        const items = []
        if (q.length < 2) return items
        const lower = q.toLowerCase()
        const models = [appModel].concat(webModels)
        for (const m of models) {
            for (let r = 0; r < m.count; ++r) {
                const name = m.nameAt(r)
                if (name.toLowerCase().indexOf(lower) >= 0) {
                    const icon = m.iconAt(r)
                    items.push({ name: name, detail: "", icon: icon.startsWith("glyph:") ? icon : icon, section: qsTr("Apps en diensten"),
                                 kind: "app", model: m, row: r, badge: parental.enabled && parental.isLocked(m.pathAt(r)) ? "🔒" : "" })
                }
            }
        }
        for (let r = 0; r < radio.count; ++r) {
            if (radio.isStation(r) && radio.nameAt(r).toLowerCase().indexOf(lower) >= 0) {
                items.push({ name: radio.nameAt(r), detail: radio.subtitleAt(r), icon: radio.iconAt(r), section: qsTr("Radio"), kind: "radio", row: r })
            }
        }
        const lib = []
        for (const item of library.movies.concat(library.shows)) {
            if ((item.title || "").toLowerCase().indexOf(lower) >= 0) {
                lib.push({ name: item.title, detail: (item.kind === "show" ? qsTr("Serie") : qsTr("Film")) + (item.year ? " · " + item.year : ""),
                           icon: item.localPoster || item.poster || "glyph:\uE8B2", section: qsTr("Films en series"), kind: "library", item: item })
            }
        }
        items.push(...lib.slice(0, 20))
        for (const r of iptv.searchAll(q, 25)) {
            const programme = r.kind === "programme"
            const scheduled = programme && recorder.isScheduled(r.url, r.startSecs)
            items.push({ name: r.name, detail: r.detail, icon: r.icon || "glyph:\uE7F4",
                         section: programme ? qsTr("Tv-gids") : qsTr("Live tv"), kind: r.kind, data: r,
                         badge: programme ? (r.isNow ? qsTr("Nu op tv") : scheduled ? qsTr("● Opname gepland") : qsTr("Menu: opnemen")) : "" })
        }
        return items
    }

    SearchPicker {
        id: picker
        onQuery: (q) => {
            if (root.pickerMode === "radio") {
                radio.searchStations(q)
            } else if (root.pickerMode === "photo") {
                photos.search(q)
            } else if (root.pickerMode === "name") {
                picker.results = q.length > 0 ? [{ name: qsTr("Opslaan als “") + q + "”", detail: qsTr("Enter om het profiel te maken"), icon: "glyph:\uE77B" }] : []
            } else if (root.pickerMode === "place") {
                if (q.length >= 2) weather.searchPlaces(q)
            } else {
                root.pickerItems = root.searchAll(q)
                picker.results = root.pickerItems
                console.info("Search", q, root.pickerItems.length)
            }
        }
        onPicked: (index) => root.pickResult(index)
        onSecondary: (index) => root.pickResult(index, true)
    }
    Connections {
        target: radio
        function onResultsChanged() {
            if (root.pickerMode !== "radio") return
            picker.busy = radio.searching
            picker.results = radio.results.map((r, i) => ({ name: r.name, detail: r.detail, icon: r.logo,
                                                             badge: radio.hasResult(i) ? qsTr("✓ Toegevoegd") : "" }))
        }
    }
    Connections {
        target: photos
        function onResultsChanged() {
            if (root.pickerMode !== "photo") return
            picker.busy = photos.busy
            picker.results = photos.results.map(p => ({ name: p.name, detail: p.detail, icon: p.thumb }))
        }
        function onDownloaded(file, credit) {
            if (appModel.setBackground(file)) {
                picker.close()
                root.showToast(qsTr("Nieuwe achtergrond. ") + credit, 6000)
            }
        }
        function onFailed(message) { root.showToast(message, 5000) }
    }
    Connections {
        target: weather
        function onPlacesChanged() {
            if (root.pickerMode !== "place") return
            picker.results = weather.places.map(p => ({ name: p.name, detail: p.detail, icon: "📍" }))
        }
    }

    function pickResult(index, secondary) {
        if (index < 0) return
        if (pickerMode === "radio") {
            if (radio.addResult(index)) {
                showToast(picker.nameAt(index) + qsTr(" staat nu in de rij Radio"))
                picker.results = radio.results.map((r, i) => ({ name: r.name, detail: r.detail, icon: r.logo,
                                                                 badge: radio.hasResult(i) ? qsTr("✓ Toegevoegd") : "" }))
            } else {
                showToast(picker.nameAt(index) + qsTr(" staat al in de rij Radio"))
            }
            return
        }
        if (pickerMode === "name") {
            const name = picker.text.trim()
            if (name === "") return
            const id = profiles.add(name, pendingKidsProfile)
            picker.close()
            menu.open(qsTr("Profiel %1 is gemaakt").arg(name), [
                { label: qsTr("Nu naar ") + name + qsTr(" wisselen"), action: "profile-switch:" + id },
                { label: qsTr("Later"), action: "" }
            ], root.menuFocus())
            return
        }
        if (pickerMode === "photo") {
            showToast(qsTr("Foto wordt opgehaald…"), 20000)
            photos.download(index)
            return
        }
        if (pickerMode === "place") {
            weather.choosePlace(index)
            picker.close()
            showToast(qsTr("Weer voor ") + picker.nameAt(index))
            return
        }
        const it = pickerItems[index]
        if (!it) return
        if (it.kind === "programme" && (secondary || !it.data.isNow)) {
            // Toekomstig programma: opnemen in- of uitplannen
            const d = it.data
            recorder.toggleSchedule(d.channel, d.url, d.name, d.startSecs, d.stopSecs)
            const on = recorder.isScheduled(d.url, d.startSecs)
            showToast(on ? qsTr("● Wordt opgenomen: ") + d.name : qsTr("Opname geannuleerd: ") + d.name, 5000)
            pickerItems[index].badge = on ? qsTr("● Opname gepland") : qsTr("Menu: opnemen")
            picker.results = pickerItems
            return
        }
        picker.visible = false
        const back = picker.returnFocusTo
        if (back) back.forceActiveFocus()
        if (it.kind === "app") {
            launch(it.model, it.row)
        } else if (it.kind === "radio") {
            radioPlayer.open(it.row, back || quickRow)
        } else if (it.kind === "library") {
            const open = () => {
                libraryScreen.open(it.item.kind === "show" ? "shows" : "movies", back || mediaGrid())
                libraryScreen.showDetail(it.item)
            }
            const path = "bigscreen:movies"
            if (parental.needsPin(path)) {
                pinPad.ask(it.name + qsTr(" is vergrendeld"), qsTr("Vul de pincode van het kinderslot in."), (pin) => { if (!parental.check(pin)) return false; open(); return true }, back || quickRow)
            } else {
                open()
            }
        } else {
            const playChannel = () => {
                radioPlayer.stopPlayback()
                iptvScreen.open(back || mediaGrid())
                const row = iptv.showChannel(it.data.url)
                if (row >= 0) iptvScreen.play(row)
            }
            if (parental.needsPin("bigscreen:iptv")) {
                pinPad.ask(qsTr("Live tv is vergrendeld"), qsTr("Vul de pincode van het kinderslot in."), (pin) => { if (!parental.check(pin)) return false; playChannel(); return true }, back || quickRow)
            } else {
                playChannel()
            }
        }
    }

    PinPad {
        id: pinPad
    }

    RemoteOverlay {
        id: remoteOverlay
    }

    // --- Herinnering: melding 2 minuten voor een programma, ook als er iets anders voorop staat ---
    property var reminderData: null
    Connections {
        target: iptv
        function onReminderDue(r) {
            root.reminderData = r
            reminderTimer.restart()
            homeBridge.raiseWindow()
            Qt.callLater(() => reminderWatch.forceActiveFocus())
        }
    }
    Timer { id: reminderTimer; interval: 90000; onTriggered: root.reminderData = null }
    Rectangle {
        id: reminderCard
        visible: root.reminderData !== null
        z: 350
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: root.unit * 3
        width: root.unit * 62
        height: reminderColumn.implicitHeight + root.unit * 5
        radius: root.unit * 1.5
        color: "#f01d2026"
        border.color: "#ffb74d"
        border.width: 2
        Column {
            id: reminderColumn
            x: root.unit * 2.5
            y: root.unit * 2.5
            width: parent.width - root.unit * 5
            spacing: root.unit * 1.2
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                color: "#ffb74d"
                font.pixelSize: root.unit * 2.2 * appModel.fontScale
                text: root.reminderData ? qsTr("🔔  Om ") + root.reminderData.time + qsTr(" op ") + root.reminderData.channel : ""
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                wrapMode: Text.Wrap
                color: "white"
                font.pixelSize: root.unit * 3.2 * appModel.fontScale
                font.weight: Font.DemiBold
                text: root.reminderData ? root.reminderData.title : ""
            }
            Row {
                spacing: root.unit * 1.2
                FocusButton {
                    id: reminderWatch
                    unit: root.unit
                    text: qsTr("Kijken")
                    onClicked: {
                        const r = root.reminderData
                        root.reminderData = null
                        if (r) {
                            if (homeBridge.external) homeBridge.goHome()
                            root.watchChannel(r.url, root.menuFocus())
                        }
                    }
                    KeyNavigation.right: reminderLater
                    Keys.onEscapePressed: reminderLater.clicked()
                }
                FocusButton {
                    id: reminderLater
                    unit: root.unit
                    text: qsTr("Sluiten")
                    onClicked: { root.reminderData = null; root.menuFocus().forceActiveFocus() }
                    KeyNavigation.left: reminderWatch
                    Keys.onEscapePressed: clicked()
                }
            }
        }
    }

    // --- Sfeerstand: na een tijd niets doen vanzelf naar Schilderij ---
    Timer {
        id: ambientTimer
        interval: selfTest === "ambient" ? 2000 : 20000
        repeat: true
        running: true
        onTriggered: {
            const minutes = selfTest === "ambient" ? 0.05 : appModel.setting("ambientMinutes", 10)
            if (minutes <= 0 || systemTools.idleSeconds() < minutes * 60) return
            if (Qt.application.state !== Qt.ApplicationActive && selfTest !== "ambient") return
            if (homeBridge.external || frameScreen.visible || iptvScreen.visible || libraryScreen.visible || mpc.running
                || menu.visible || picker.visible || pinPad.visible || welcomeOverlay.visible || resumeScreen.visible
                || remoteOverlay.visible || kidsLock.visible || reminderCard.visible || recorder.recording && iptvScreen.visible) return
            console.info("Ambient mode: frame after", systemTools.idleSeconds(), "s")
            radioPlayer.visible && radioPlayer.close()
            frameScreen.open(root.menuFocus())
        }
    }

    // --- Kijktijd kinderprofiel: minuten per dag tellen, daarna pincode ---
    Timer {
        id: kidsTimer
        interval: 60000
        repeat: true
        running: profiles.kids
        property bool warned: false
        function today() { return Qt.formatDate(new Date(), "yyyy-MM-dd") }
        function used() { return parental.kidsUsed(today()) }
        function check() {
            // Oudere versies bewaarden de kijktijd per profiel in settings.ini: eenmalig overnemen
            const old = appModel.setting("kidsMinutes", 0)
            if (old > 0 && parental.kidsMinutes() === 0) { parental.setKidsMinutes(old); appModel.setSetting("kidsMinutes", 0) }
            const limit = parental.kidsMinutes()
            if (!profiles.kids || limit <= 0) return
            const left = limit - used() + parental.kidsExtra(today())
            if (left <= 5 && left > 0 && !warned) {
                warned = true
                if (homeBridge.external) homeBridge.raiseWindow()
                root.showToast(qsTr("Nog %1 minuten kijktijd voor vandaag").arg(left), 8000)
            }
            if (left <= 0 && !kidsLock.visible) {
                console.info("Kids limit reached", used(), limit)
                if (homeBridge.external) homeBridge.goHome()
                iptvScreen.visible && iptvScreen.close()
                libraryScreen.visible && libraryScreen.close()
                radioPlayer.stopPlayback()
                kidsLock.open()
            }
        }
        onTriggered: {
            // Alleen tellen als er iets gebeurt: Kijkluister voorop of een dienst/film open
            if (Qt.application.state === Qt.ApplicationActive || homeBridge.external || mpc.running) {
                parental.setKidsUsed(today(), used() + 1)
            }
            check()
        }
    }
    FocusScope {
        id: kidsLock
        anchors.fill: parent
        visible: false
        z: 380
        function open() { visible = true; kidsLockButton.forceActiveFocus() }
        Rectangle { anchors.fill: parent; color: "#f00b0c0f"; MouseArea { anchors.fill: parent } }
        Column {
            anchors.centerIn: parent
            spacing: root.unit * 2.5
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.horizontalCenter: parent.horizontalCenter
                color: "white"
                font.pixelSize: root.unit * 5 * appModel.fontScale
                font.weight: Font.DemiBold
                text: qsTr("De kijktijd voor vandaag is op")
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.horizontalCenter: parent.horizontalCenter
                color: "#c9ccd1"
                font.pixelSize: root.unit * 2.6 * appModel.fontScale
                text: qsTr("Morgen kan ") + profiles.currentName + qsTr(" weer kijken. Met de pincode kan het nog een half uur.")
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: root.unit * 1.5
                FocusButton {
                    id: kidsLockButton
                    unit: root.unit
                    text: qsTr("Pincode: half uur erbij")
                    onClicked: pinPad.ask(qsTr("Nog een half uur"), qsTr("Vul de pincode van het kinderslot in."), (pin) => {
                        if (!parental.check(pin)) return false
                        parental.setKidsExtra(kidsTimer.today(), parental.kidsExtra(kidsTimer.today()) + 30)
                        kidsTimer.warned = false
                        kidsLock.visible = false
                        root.menuFocus().forceActiveFocus()
                        return true
                    }, kidsLockButton)
                    KeyNavigation.right: kidsSwitchButton
                }
                FocusButton {
                    id: kidsSwitchButton
                    visible: profiles.count > 1
                    unit: root.unit
                    text: qsTr("Ander profiel")
                    onClicked: root.openProfileMenu()
                    KeyNavigation.left: kidsLockButton
                }
            }
        }
        Keys.onPressed: (event) => { event.accepted = true }
    }

    // --- Bij opstarten: wie kijkt er? ---
    function askWhoIsWatching() {
        const entries = profiles.list.map(p => ({ label: p.name + (p.kids ? qsTr("  (kind)") : "") + (p.current ? "   ✓" : ""),
                                                  action: p.current ? "" : "profile-switch:" + p.id }))
        menu.open(qsTr("Wie kijkt er?"), entries, root.menuFocus(), qsTr("Uitzetten kan in Instellingen → Gezin en veiligheid."))
        console.info("Who is watching:", profiles.count)
    }

    ReleaseInfo {
        id: releaseInfo
    }

    HelpOverlay {
        id: helpOverlay
    }

    // Nachtmodus: 22-7 uur (of altijd) gedimd en warmer beeld, radio zachter, slaaptimer in de bovenbalk
    property bool nightActive: false
    function updateNight() {
        const mode = appModel.setting("night", "auto")
        const h = new Date().getHours()
        nightActive = selfTest === "night" || mode === "on" || (mode === "auto" && (h >= 22 || h < 7))
    }
    Timer {
        interval: 60000
        repeat: true
        running: true
        triggeredOnStart: true
        onTriggered: root.updateNight()
    }
    Connections { target: appModel; function onSettingsChanged() { root.updateNight() } }
    onNightActiveChanged: {
        console.info("Night mode", nightActive)
        let index = -1
        for (let i = 0; i < quickModel.count; ++i) if (quickModel.get(i).uri === "sleep") index = i
        if (nightActive && index < 0) {
            quickModel.insert(1, { label: qsTr("Slaaptimer"), code: 0xE708, uri: "sleep", about: qsTr("Radio, tv en films stoppen vanzelf") })
        } else if (!nightActive && index >= 0) {
            quickModel.remove(index)
        }
        radioPlayer.maxVolume = nightActive ? 0.5 : 1.0
    }
    Rectangle {
        anchors.fill: parent
        z: 400
        visible: root.nightActive
        color: "#3a1a0c00"   // warm en iets donkerder
        enabled: false
    }

    // Profielknop in de bovenbalk zodra er meer dan één profiel is
    Connections {
        target: profiles
        function onChanged() { root.syncProfileButton() }
    }
    function syncProfileButton() {
        let index = -1
        for (let i = 0; i < quickModel.count; ++i) if (quickModel.get(i).uri === "profile") index = i
        if (profiles.count > 1 && index < 0) {
            quickModel.insert(0, { label: profiles.currentName, code: 0xE77B, uri: "profile", about: qsTr("Ander profiel kiezen") })
        } else if (profiles.count <= 1 && index >= 0) {
            quickModel.remove(index)
        }
    }

    // Opstarten waar je was: kader op de laatst geopende tegel; radio eventueel hervatten
    Timer {
        id: startWhereLeft
        interval: 400
        onTriggered: {
            root.syncProfileButton()
            kidsTimer.check()
            if ((selfTest === "" || selfTest === "whoswatching") && profiles.count > 1 && systemTools.globalSetting("askProfile", true)
                && !systemTools.hasArgument("--profile-chosen") && !welcomeOverlay.visible) {
                root.askWhoIsWatching()
                return
            }
            const cat = usage.lastCategory
            const src = cat !== "" ? usage.sourceModel(cat) : null
            const row = src ? usage.sourceRow(cat, usage.lastPath) : -1
            if (cat === "radio" && row >= 0 && appModel.setting("resumeRadio", false) && selfTest === "") {
                radioPlayer.open(row, mediaGrid())
                radioPlayer.close()
            }
            const sec = src ? sectionFor(src) : null
            if (sec && row >= 0 && !welcomeOverlay.visible && selfTest === "") {
                sec.grid.currentIndex = row
                sec.grid.forceActiveFocus()
                console.info("Start where left:", src.nameAt(row))
            }
        }
    }

    // Tegel Verder kijken (rij Media, naast Schilderij)
    ResumeScreen {
        id: resumeScreen
        onPlayRequested: (row) => { resumeScreen.visible = false; root.launch(resume, row) }
        onMenuRequested: (row) => {
            editModel = resume
            editRow = row
            menu.open(resume.nameAt(row), [
                { label: qsTr("Verder kijken"), action: "resume-play" },
                { label: qsTr("Uit de lijst halen"), action: "resume-remove" },
                { label: qsTr("Annuleren"), action: "" }
            ], resumeScreen)
        }
    }
    Connections {
        target: resume
        function onCountChanged() { appModel.setResumeCount(resume.count) }
        Component.onCompleted: appModel.setResumeCount(resume.count)
    }

    // Stil bijwerken: klein bericht rechtsonder; installeren zodra er niets speelt
    Rectangle {
        id: updateBanner
        readonly property bool shown: updater.phase !== "" && !iptvScreen.visible && !libraryScreen.visible && !frameScreen.visible
                                      && !radioPlayer.visible && !picker.visible
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: root.unit * 2
        anchors.bottomMargin: root.unit * 7
        z: 40
        width: updateText.implicitWidth + root.unit * 4
        height: root.unit * 5
        radius: height / 2
        color: "#e61d2a40"
        border.color: "#55ffffff"
        border.width: 1
        opacity: shown ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 300 } }
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            id: updateText
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: root.unit * 2 * appModel.fontScale
            text: updater.phase === "downloading" ? qsTr("⬇  Nieuwe versie ") + updater.latest + qsTr(" wordt op de achtergrond gedownload…")
                : updater.phase === "ready" ? qsTr("✓  Kijkluister ") + updater.latest + qsTr(" wordt geïnstalleerd zodra er niets speelt")
                : qsTr("⟳  Kijkluister ") + updater.latest + qsTr(" wordt geïnstalleerd en start zo opnieuw…")
        }
    }
    Timer {
        interval: 10000
        repeat: true
        running: updater.phase === "ready"
        onTriggered: {
            const idle = !radioPlayer.active && !iptvScreen.visible && !libraryScreen.visible && !mpc.running && !recorder.recording
                         && !homeBridge.external && !menu.visible && !picker.visible && !pinPad.visible && !frameScreen.visible
            if (idle) updater.installNow()
        }
    }

    // Zwevende Home-knop linksboven zolang een app of website voorop staat
    HomeButton {}

    // Home-toets, Home-knop, Esc lang ingedrukt of Guide op de controller: terug naar de tegels
    Connections {
        target: gamepad
        function onHomeRequested() { root.goHomeScreen() }
    }
    // Alleen voor de tests van de tabbladen-layout: na menu's en omschakelen moet de focus op een zichtbaar item staan
    function tabsTest(mode) {
        const focusOk = (what) => {
            const f = root.Window.window ? root.Window.window.activeFocusItem : null
            const inGrid = currentSection !== null && currentSection.grid.activeFocus
            console.info("Tabs focus", what, "ok", f !== null && f.visible, "grid", inGrid, "tabs", tabsMode, "sections", sectionDefs.length,
                         "current", currentSection !== null, "item", f ? String(f).split("(")[0] : "none")
        }
        const tabIndex = (id) => visibleTabs.findIndex(t => t.id === id)
        console.info("Tabs visible:", visibleTabs.map(t => t.id).join(","), "first section", sectionDefs.length > 0 ? sectionDefs[0].title : "-")
        if (mode === "tabs-apps" || mode === "tabs-tv") {
            tabList.forceActiveFocus()
            selectTab(tabIndex(mode === "tabs-apps" ? "apps" : "tv"))
            Qt.callLater(() => {
                if (mode === "tabs-tv") liveButton.forceActiveFocus()
                else focusSection(0, 0, false)
                testStep.run(1500, () => focusOk(mode))
            })
            return
        }
        focusSection(0, 0, false)
        testStep.run(1200, () => {
            focusOk("content")
            // Tabblad wisselen en terug
            tabList.forceActiveFocus()
            selectTab(1)
            selectTab(0)
            Qt.callLater(() => {
                focusSection(0, 0, false)
                // Tegelmenu openen en sluiten
                testStep.run(800, () => {
                    openTileMenu(sectionDefs[0].model, 0, currentSection.grid)
                    testStep.run(1000, () => {
                        menu.close()
                        testStep.run(800, () => {
                            focusOk("after-menu")
                            // Terug naar klassiek en weer naar tabbladen
                            handleMenuAction("layout")
                            testStep.run(1500, () => {
                                focusOk("classic")
                                handleMenuAction("layout")
                                testStep.run(1500, () => {
                                    focusSection(0, 0, false)
                                    testStep.run(800, () => focusOk("tabs-again"))
                                })
                            })
                        })
                    })
                })
            })
        })
    }

    function goHomeScreen() {
        console.info("Home screen")
        if (menu.visible) menu.close()
        if (picker.visible) picker.close()
        if (pinPad.visible) pinPad.close()
        if (radioPlayer.visible) radioPlayer.close()
        if (iptvScreen.visible) iptvScreen.close()
        if (libraryScreen.visible) libraryScreen.close()
        if (frameScreen.visible) frameScreen.close()
        if (resumeScreen.visible) resumeScreen.close()
        if (releaseInfo.visible) releaseInfo.close()
        Qt.callLater(() => focusSection(0, 0, false))
    }

    // Alleen voor de tests: iets later uitvoeren
    Timer {
        id: testStep
        property var action: null
        function run(ms, fn) { action = fn; interval = ms; restart() }
        onTriggered: if (action) action()
    }

    // Opruimen van opnames: Kijkluister vraagt het eerst
    Connections {
        target: recorder
        function onCleanupSuggested(count, reason) {
            if (iptvScreen.visible || libraryScreen.visible || radioPlayer.visible || menu.visible) {
                return   // later nog eens (de opnemer vraagt het hooguit eens per dag)
            }
            root.askCleanup(false)
        }
    }

    // Nieuwe versie
    Connections {
        target: updater
        function onChecked(manual) {
            if (updater.silent && updater.updateAvailable) {
                if (manual) root.showToast(qsTr("Kijkluister ") + updater.latest + qsTr(" wordt op de achtergrond opgehaald en geïnstalleerd"), 6000)
            } else if (updater.shouldOffer() || (manual && updater.updateAvailable)) {
                menu.open(qsTr("Kijkluister ") + updater.latest + qsTr(" is er"), [
                    { label: qsTr("Nu bijwerken"), action: "update-install" },
                    { label: qsTr("Later"), action: "update-later" }
                ], root.currentSection ? root.currentSection.grid : quickRow,
                   qsTr("Je hebt nu ") + updater.current + qsTr(". Bijwerken duurt ongeveer een minuut; daarna start Kijkluister vanzelf weer."))
            } else if (manual) {
                root.showToast(updater.status, 6000)
            }
        }
    }
    Connections {
        target: updater
        function onChanged() {
            if (updater.phase === "" && updater.busy && updater.progress > 0 && updater.status !== "") {
                root.showToast(updater.status + (updater.progress > 0 ? "  " + Math.round(updater.progress * 100) + "%" : ""), 60000)
            }
        }
    }
    Connections {
        target: cec
        function onChanged() {
            if (cec.connected) root.showToast(qsTr("📺 Tv-afstandsbediening verbonden"), 4000)
        }
    }
    // Test: volgende zender zodra deze speelt of echt niet lukt (reserve-adressen krijgen de tijd)
    Timer {
        id: radioTestTimer
        property int waited: 0
        interval: 1000
        repeat: true
        onTriggered: {
            waited += 1
            if (waited < 4 || (radioPlayer.loading && waited < 25)) {
                return
            }
            waited = 0
            const next = radio.nextStation(radioPlayer.row, 1)
            if (next <= radioPlayer.row) {
                stop()
            } else {
                radioPlayer.tune(next)
            }
        }
    }

    // Films en Series: eigen scherm met de bibliotheek
    LibraryScreen {
        id: libraryScreen
    }

    Connections {
        target: appModel
        function onInternalRequested(target) {
            if (target === "bigscreen:resume") {
                resumeScreen.open(mediaGrid())
            } else if (target === "bigscreen:radio-now") {
                radioPlayer.show(mediaGrid())
            } else if (target === "bigscreen:regional") {
                iptvScreen.open(mediaGrid(), "Regionale tv")
            } else if (target === "bigscreen:iptv") {
                iptvScreen.open(mediaGrid())
            } else if (target === "bigscreen:frame") {
                frameScreen.open(mediaGrid())
            } else {
                libraryScreen.open(target === "bigscreen:shows" ? "shows" : "movies", mediaGrid())
            }
        }
    }
}
