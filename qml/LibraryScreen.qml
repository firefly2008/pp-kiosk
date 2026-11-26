// Bibliotheek: films en series uit de eigen mappen, met informatie die op dezelfde
// manier wordt opgehaald als de Kodi-scrapers dat doen (zie medialibrary.h).
// Indeling: menu links, grote titel en beschrijving boven, posters onder; Enter
// opent het detailscherm met achtergrond, poster, cast en (bij series) afleveringen.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

FocusScope {
    id: lib

    readonly property real unit: height / 100
    property string section: "movies"
    readonly property var items: section === "movies" ? library.movies : library.shows
    property var detail: null
    property string autoOpen: ""          // alleen voor de geautomatiseerde test
    property Item returnFocusTo: null
    property bool scannedThisSession: false
    property int savedIndex: 0
    property bool restoring: false

    readonly property var current: detail !== null ? detail
                                  : (grid.currentIndex >= 0 && grid.currentIndex < items.length ? items[grid.currentIndex] : null)

    // De aflevering die nu focus heeft in de serie-detail (voor plot + achtergrond)
    readonly property var focusedEpisode:
        (detail && detail.kind === "show" && detail.episodes
         && episodeList.activeFocus && episodeList.currentIndex >= 0
         && episodeList.currentIndex < detail.episodes.length)
        ? detail.episodes[episodeList.currentIndex] : null

    anchors.fill: parent
    visible: false
    z: 50

    function open(which, focusItem) {
        section = which
        returnFocusTo = focusItem
        detail = null
        savedIndex = 0
        visible = true
        if (!library.configured) {
            setup.open(true)
            return
        }
        if (!scannedThisSession) {
            scannedThisSession = true
            library.scan(false)
        }
        grid.currentIndex = 0
        grid.forceActiveFocus()
    }

    function openSetup() {
        setup.open(!library.configured)
    }

    function close() {
        visible = false
        detail = null
        if (returnFocusTo) {
            returnFocusTo.forceActiveFocus()
        }
    }

    function switchSection(which) {
        if (section === which) {
            grid.forceActiveFocus()
            return
        }
        section = which
        savedIndex = 0
        grid.currentIndex = 0
    }

    function showDetail(item) {
        if (!item) {
            return
        }
        detail = item
        Qt.callLater(function() {
            if (lib.detail && lib.detail.kind === "show" && episodeList.count > 0) {
                episodeList.currentIndex = 0
                episodeList.forceActiveFocus()
            } else {
                playButton.forceActiveFocus()
            }
        })
    }

    function hideDetail() {
        detail = null
        grid.forceActiveFocus()
    }

    function play(path) {
        message.text = library.play(path) ? qsTr("Afspelen…") : qsTr("Bestand niet gevonden")
        messageTimer.restart()
    }

    // Serie: vanaf deze aflevering alles achter elkaar
    function playFrom(index) {
        const eps = detail && detail.episodes ? detail.episodes : []
        const paths = []
        for (let i = Math.max(0, index); i < eps.length; ++i) {
            paths.push(eps[i].path)
        }
        message.text = paths.length > 0 && library.playEpisodes(paths) ? qsTr("Afspelen…") : qsTr("Bestand niet gevonden")
        messageTimer.restart()
    }

    function formatRuntime(minutes) {
        if (!minutes) {
            return ""
        }
        const h = Math.floor(minutes / 60)
        const m = minutes % 60
        return h > 0 ? h + "u " + (m < 10 ? "0" : "") + m + "m" : m + "m"
    }

    function episodeMeta(e) {
        if (!e) {
            return ""
        }
        const parts = []
        if (e.air) {
            const d = new Date(e.air)
            parts.push(isNaN(d.getTime()) ? e.air : Qt.formatDate(d, "d MMM yyyy"))
        }
        if (e.runtime) {
            parts.push(formatRuntime(e.runtime))
        }
        if (e.rating) {
            parts.push("★ " + Number(e.rating).toFixed(1))
        }
        return parts.join("   ·   ")
    }

    function metaLine(item) {
        if (!item) {
            return ""
        }
        const parts = []
        if (item.rating) {
            parts.push("★ " + Number(item.rating).toFixed(1))
        }
        if (item.year) {
            parts.push(item.year)
        }
        if (item.kind === "movie" && item.runtime) {
            parts.push(formatRuntime(item.runtime))
        }
        if (item.kind === "show" && item.episodes) {
            parts.push(item.episodes.length + (item.episodes.length === 1 ? qsTr(" aflevering") : qsTr(" afleveringen")))
        }
        if (item.genres) {
            parts.push(item.genres)
        }
        return parts.join("    ·    ")
    }

    // Lijst ververst tijdens het ophalen: selectie op dezelfde plek houden
    onItemsChanged: {
        restoring = true
        Qt.callLater(function() {
            grid.currentIndex = Math.max(0, Math.min(lib.savedIndex, lib.items.length - 1))
            lib.restoring = false
            if ((lib.autoOpen === "detail" || lib.autoOpen === "play") && lib.detail === null && lib.items.length > 0 && !library.busy) {
                const playToo = lib.autoOpen === "play"
                lib.autoOpen = ""
                lib.showDetail(lib.items[0])
                if (playToo) {
                    lib.playFrom(0)
                }
            }
        })
    }

    // Esc/Terug: één stap terug (instellingen → detail → lijst → startscherm), waar de focus ook staat
    function back() {
        if (setup.visible) {
            setup.close()
            if (!library.configured) lib.close()
            else grid.forceActiveFocus()
        } else if (lib.detail !== null) {
            lib.hideDetail()
        } else {
            lib.close()
        }
        console.info("Library back; visible", lib.visible)
    }
    Keys.onEscapePressed: lib.back()
    Keys.onBackPressed: lib.back()
    // Ook als een knop, zoekveld of lijst de toets zelf zou houden, of de focus kwijt is
    Shortcut {
        sequences: ["Esc", "Back"]
        context: Qt.WindowShortcut
        enabled: lib.visible
        onActivated: lib.back()
    }

    // --- Achtergrond: afbeelding van de gekozen titel, links en onder donker ---
    Rectangle {
        anchors.fill: parent
        color: "#0b0c0f"
    }
    Image {
        id: backdrop
        anchors.fill: parent
        source: lib.focusedEpisode && lib.focusedEpisode.still
                ? lib.focusedEpisode.still
                : (lib.current ? (lib.current.backdrop || "") : "")
        sourceSize.width: 1920
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        opacity: status === Image.Ready ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 300 } }
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#f20b0c0f" }
            GradientStop { position: 0.45; color: "#b00b0c0f" }
            GradientStop { position: 1.0; color: "#200b0c0f" }
        }
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#000b0c0f" }
            GradientStop { position: lib.detail !== null ? 0.5 : 0.35; color: "#600b0c0f" }
            GradientStop { position: 1.0; color: "#f80b0c0f" }
        }
    }

    // =====================================================================
    // Overzicht: menu, grote titel, posters
    // =====================================================================
    Item {
        id: overview
        anchors.fill: parent
        visible: lib.detail === null

        // --- Menu links ---
        Rectangle {
            id: sidebar
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: lib.unit * 30
            color: "#c00b0c0f"

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                id: sidebarTitle
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: lib.unit * 4
                color: "white"
                font.pixelSize: (lib.unit * 3.6) * appModel.fontScale
                font.weight: Font.DemiBold
                text: qsTr("Bibliotheek")
            }

            ListView {
                id: menu
                anchors.top: sidebarTitle.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.topMargin: lib.unit * 4
                anchors.leftMargin: lib.unit * 2
                anchors.rightMargin: lib.unit * 2
                height: count * lib.unit * 7
                interactive: false
                keyNavigationEnabled: true
                model: [
                    { label: qsTr("Films"), glyph: "", action: "movies" },
                    { label: qsTr("Series"), glyph: "", action: "shows" },
                    { label: qsTr("Bijwerken"), glyph: "", action: "scan" },
                    { label: qsTr("Mappen kiezen"), glyph: "", action: "setup" },
                    { label: qsTr("Terug"), glyph: "", action: "back" }
                ]

                function trigger(index) {
                    const action = model[index].action
                    if (action === "movies" || action === "shows") {
                        lib.switchSection(action)
                        grid.forceActiveFocus()
                    } else if (action === "scan") {
                        library.scan(false)
                    } else if (action === "setup") {
                        setup.open(false)
                    } else {
                        lib.close()
                    }
                }

                delegate: Rectangle {
                    id: entry
                    required property int index
                    required property var modelData
                    readonly property bool focused: menu.activeFocus && menu.currentIndex === index
                    readonly property bool activeSection: modelData.action === lib.section
                    width: menu.width
                    height: lib.unit * 6.4
                    radius: height / 2
                    color: focused ? "#e8eaed" : (activeSection ? "#2a2d33" : "transparent")

                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: lib.unit * 2.2
                        spacing: lib.unit * 1.8
                        Text {
                            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                            anchors.verticalCenter: parent.verticalCenter
                            color: entry.focused ? "#111318" : "white"
                            font.family: "Segoe MDL2 Assets"
                            font.pixelSize: lib.unit * 2.6
                            text: entry.modelData.glyph
                        }
                        Text {
                            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                            anchors.verticalCenter: parent.verticalCenter
                            color: entry.focused ? "#111318" : "white"
                            font.pixelSize: (lib.unit * 2.5) * appModel.fontScale
                            font.weight: entry.activeSection ? Font.DemiBold : Font.Normal
                            text: entry.modelData.label
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            menu.currentIndex = entry.index
                            menu.forceActiveFocus()
                            menu.trigger(entry.index)
                        }
                    }
                }

                Keys.onReturnPressed: trigger(currentIndex)
                Keys.onEnterPressed: trigger(currentIndex)
                Keys.onRightPressed: grid.forceActiveFocus()
            }

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: lib.unit * 3
                wrapMode: Text.Wrap
                color: "#6f737a"
                font.pixelSize: (lib.unit * 1.6) * appModel.fontScale
                text: qsTr("Gegevens: TMDB en TVmaze. Dit product gebruikt de TMDB API, maar is niet door TMDB goedgekeurd of gecertificeerd.")
            }
        }

        // --- Grote titel en beschrijving van de gekozen poster ---
        Column {
            id: hero
            anchors.top: parent.top
            anchors.left: sidebar.right
            anchors.leftMargin: lib.unit * 5
            anchors.topMargin: lib.unit * 6
            width: parent.width * 0.55
            spacing: lib.unit * 1.2
            visible: lib.current !== null

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                color: "white"
                font.pixelSize: (lib.unit * 6.5) * appModel.fontScale
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                text: lib.current ? lib.current.title : ""
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                color: "#d0d3d8"
                font.pixelSize: (lib.unit * 2.3) * appModel.fontScale
                elide: Text.ElideRight
                text: lib.metaLine(lib.current)
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                wrapMode: Text.Wrap
                maximumLineCount: 4
                elide: Text.ElideRight
                color: "#e8eaed"
                font.pixelSize: (lib.unit * 2.3) * appModel.fontScale
                lineHeight: 1.15
                text: lib.current ? (lib.current.overview || "") : ""
            }
        }

        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            id: statusLine
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: lib.unit * 3
            color: "#9fb4d0"
            font.pixelSize: (lib.unit * 2) * appModel.fontScale
            text: library.status
        }

        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            id: sectionTitle
            anchors.bottom: grid.top
            anchors.left: grid.left
            anchors.leftMargin: lib.unit * 1.5
            color: "white"
            font.pixelSize: (lib.unit * 2.8) * appModel.fontScale
            font.weight: Font.DemiBold
            text: lib.section === "movies" ? qsTr("Films") : qsTr("Series")
        }

        // --- Posters ---
        GridView {
            id: grid
            anchors.left: sidebar.right
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: lib.unit * 2
            anchors.rightMargin: lib.unit * 2
            height: lib.unit * 56
            topMargin: lib.unit * 2
            bottomMargin: lib.unit * 2
            // Ongeveer zes posters naast elkaar op een tv-scherm
            readonly property int columns: Math.max(4, Math.round(width / (lib.unit * 24)))
            cellWidth: Math.floor(width / columns)
            cellHeight: Math.floor(cellWidth * 1.66)
            clip: true
            focus: true
            keyNavigationEnabled: true
            highlightMoveDuration: 150
            model: lib.items

            onCurrentIndexChanged: {
                if (!lib.restoring) {
                    lib.savedIndex = currentIndex
                }
            }

            delegate: PosterTile {
                fontSize: lib.unit * 1.9
                width: grid.cellWidth
                height: grid.cellHeight
                selected: grid.activeFocus && grid.currentIndex === index
                onActivated: {
                    grid.currentIndex = index
                    grid.forceActiveFocus()
                    lib.showDetail(lib.items[index])
                }
                onHovered: {
                    if (!setup.visible) {
                        grid.currentIndex = index
                        grid.forceActiveFocus()
                    }
                }
            }

            Keys.onReturnPressed: lib.showDetail(lib.items[currentIndex])
            Keys.onEnterPressed: lib.showDetail(lib.items[currentIndex])
            Keys.onLeftPressed: (event) => {
                if (currentIndex % columns === 0) {
                    menu.currentIndex = lib.section === "movies" ? 0 : 1
                    menu.forceActiveFocus()
                } else {
                    event.accepted = false
                }
            }

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.centerIn: parent
                width: parent.width * 0.8
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: lib.items.length === 0
                color: "#9fb4d0"
                font.pixelSize: (lib.unit * 2.6) * appModel.fontScale
                text: library.busy ? qsTr("Bezig met zoeken…")
                      : lib.section === "movies"
                        ? (library.moviesFolder !== "" ? qsTr("Geen films gevonden in ") + library.moviesFolders.join(", ") : qsTr("Nog geen map met films gekozen: kies \"Mappen kiezen\""))
                        : (library.showsFolder !== "" ? qsTr("Geen series gevonden in ") + library.showsFolders.join(", ") : qsTr("Nog geen map met series gekozen: kies \"Mappen kiezen\""))
            }
        }
    }

    // =====================================================================
    // Detailscherm
    // =====================================================================
    FocusScope {
        id: detailView
        anchors.fill: parent
        visible: lib.detail !== null

        Rectangle {
            id: detailPoster
            x: lib.unit * 6
            y: lib.unit * 7
            width: lib.unit * 31
            height: width * 1.5
            radius: lib.unit
            color: "#22252b"
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.centerIn: parent
                width: parent.width * 0.85
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: detailPosterImage.status !== Image.Ready
                color: "white"
                font.pixelSize: (lib.unit * 3) * appModel.fontScale
                text: lib.detail ? lib.detail.title : ""
            }
            Image {
                id: detailPosterImage
                anchors.fill: parent
                source: lib.detail ? (lib.detail.poster || "") : ""
                sourceSize.width: 500
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
            }
        }

        Column {
            id: info
            anchors.left: detailPoster.right
            anchors.right: parent.right
            anchors.top: detailPoster.top
            anchors.leftMargin: lib.unit * 4
            anchors.rightMargin: lib.unit * 6
            spacing: lib.unit * 1.3

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                color: "white"
                font.pixelSize: (lib.unit * 5.5) * appModel.fontScale
                font.weight: Font.DemiBold
                text: lib.detail ? lib.detail.title + (lib.detail.year ? "  (" + lib.detail.year + ")" : "") : ""
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                elide: Text.ElideRight
                color: "#d0d3d8"
                font.pixelSize: (lib.unit * 2.3) * appModel.fontScale
                text: (lib.focusedEpisode && lib.episodeMeta(lib.focusedEpisode))
                      ? lib.episodeMeta(lib.focusedEpisode) : lib.metaLine(lib.detail)
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                visible: text !== ""
                elide: Text.ElideRight
                color: "#b0b4bb"
                font.pixelSize: (lib.unit * 2.2) * appModel.fontScale
                text: lib.detail && lib.detail.maker ? lib.detail.makerLabel + ": " + lib.detail.maker : ""
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                wrapMode: Text.Wrap
                maximumLineCount: lib.detail && lib.detail.kind === "show" ? 4 : 6
                elide: Text.ElideRight
                color: "#e8eaed"
                font.pixelSize: (lib.unit * 2.3) * appModel.fontScale
                lineHeight: 1.15
                text: lib.focusedEpisode && lib.focusedEpisode.overview
                      ? lib.focusedEpisode.code + "  " + lib.focusedEpisode.overview
                      : (lib.detail ? (lib.detail.overview || qsTr("Geen beschrijving gevonden.")) : "")
            }
            Item { width: 1; height: lib.unit * 0.5 }
            Row {
                spacing: lib.unit * 1.5
                FocusButton {
                    id: playButton
                    unit: lib.unit
                    text: lib.detail && lib.detail.kind === "show" ? qsTr("Eerste aflevering afspelen") : qsTr("Afspelen")
                    onClicked: {
                        if (!lib.detail) {
                            return
                        }
                        if (lib.detail.kind === "show") {
                            if (lib.detail.episodes.length > 0) {
                                lib.playFrom(0)
                            }
                        } else {
                            lib.play(lib.detail.path)
                        }
                    }
                    KeyNavigation.right: backButton
                    KeyNavigation.down: episodeList.visible ? episodeList : null
                }
                FocusButton {
                    id: backButton
                    unit: lib.unit
                    text: qsTr("Terug")
                    onClicked: lib.hideDetail()
                    KeyNavigation.left: playButton
                    KeyNavigation.down: episodeList.visible ? episodeList : null
                }
            }
        }

        // --- Afleveringen (alleen bij series) ---
        ListView {
            id: episodeList
            anchors.left: info.left
            anchors.right: info.right
            anchors.top: info.bottom
            anchors.topMargin: lib.unit * 2
            anchors.bottom: castTitle.top
            anchors.bottomMargin: lib.unit * 2
            visible: lib.detail !== null && lib.detail.kind === "show"
            model: visible ? lib.detail.episodes : []
            clip: true
            keyNavigationEnabled: true
            highlightMoveDuration: 120
            spacing: lib.unit * 0.4

            delegate: Rectangle {
                id: episodeRow
                required property int index
                required property var modelData
                readonly property bool focused: episodeList.activeFocus && episodeList.currentIndex === index
                width: episodeList.width
                height: lib.unit * 6.4
                radius: lib.unit * 0.8
                color: focused ? "#e8eaed" : (index % 2 === 0 ? "#400b0c0f" : "#200b0c0f")
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: lib.unit * 1.5
                    anchors.rightMargin: lib.unit * 1.5
                    spacing: lib.unit * 0.3
                    Row {
                        spacing: lib.unit * 2
                        Text {
                            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                            color: episodeRow.focused ? "#111318" : "#9fb4d0"
                            font.pixelSize: (lib.unit * 2.2) * appModel.fontScale
                            font.weight: Font.DemiBold
                            text: episodeRow.modelData.code
                        }
                        Text {
                            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                            width: episodeList.width - lib.unit * 16
                            elide: Text.ElideRight
                            color: episodeRow.focused ? "#111318" : "white"
                            font.pixelSize: (lib.unit * 2.2) * appModel.fontScale
                            text: episodeRow.modelData.title || qsTr("Aflevering ") + episodeRow.modelData.episode
                        }
                    }
                    Text {
                        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                        visible: text !== ""
                        width: episodeList.width - lib.unit * 3
                        elide: Text.ElideRight
                        color: episodeRow.focused ? "#3a3f47" : "#9aa0a8"
                        font.pixelSize: (lib.unit * 1.7) * appModel.fontScale
                        text: lib.episodeMeta(episodeRow.modelData)
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        episodeList.currentIndex = episodeRow.index
                        episodeList.forceActiveFocus()
                        lib.playFrom(episodeRow.index)
                    }
                }
            }

            Keys.onReturnPressed: lib.playFrom(currentIndex)
            Keys.onEnterPressed: lib.playFrom(currentIndex)
            Keys.onUpPressed: (event) => {
                if (currentIndex === 0) {
                    playButton.forceActiveFocus()
                } else {
                    event.accepted = false
                }
            }
        }

        // --- Cast ---
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            id: castTitle
            anchors.left: detailPoster.left
            anchors.bottom: castList.top
            anchors.bottomMargin: lib.unit
            visible: castList.count > 0
            color: "white"
            font.pixelSize: (lib.unit * 2.6) * appModel.fontScale
            font.weight: Font.DemiBold
            text: qsTr("Cast")
        }
        ListView {
            id: castList
            anchors.left: detailPoster.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: lib.unit * 6
            anchors.bottomMargin: lib.unit * 3
            height: lib.unit * 22
            orientation: ListView.Horizontal
            spacing: lib.unit * 1.5
            interactive: true
            model: lib.detail && lib.detail.cast ? lib.detail.cast : []

            delegate: Column {
                id: person
                required property var modelData
                width: lib.unit * 13
                spacing: lib.unit * 0.5
                Rectangle {
                    width: parent.width
                    height: width * 1.2
                    radius: lib.unit * 0.8
                    color: "#2a2d33"
                    clip: true
                    Text {
                        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                        anchors.centerIn: parent
                        visible: photo.status !== Image.Ready
                        color: "#a8acb3"
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: parent.width * 0.4
                        text: ""
                    }
                    Image {
                        id: photo
                        anchors.fill: parent
                        source: person.modelData.photo || ""
                        sourceSize.width: 200
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                    }
                }
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    width: parent.width
                    elide: Text.ElideRight
                    color: "white"
                    font.pixelSize: (lib.unit * 1.8) * appModel.fontScale
                    text: person.modelData.name
                }
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    width: parent.width
                    elide: Text.ElideRight
                    color: "#9a9ea6"
                    font.pixelSize: (lib.unit * 1.6) * appModel.fontScale
                    text: person.modelData.role || ""
                }
            }
        }
    }

    // --- Korte melding (afspelen) ---
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: lib.unit * 6
        width: message.implicitWidth + lib.unit * 6
        height: lib.unit * 7
        radius: height / 2
        color: "#e6000000"
        opacity: messageTimer.running ? 1 : 0
        visible: opacity > 0
        z: 10
        Behavior on opacity { NumberAnimation { duration: 200 } }
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            id: message
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: (lib.unit * 2.6) * appModel.fontScale
        }
    }
    Timer { id: messageTimer; interval: 2500 }

    LibrarySetup {
        id: setup
        onSaved: {
            lib.scannedThisSession = true
            grid.forceActiveFocus()
        }
        onCancelled: {
            if (!library.configured) {
                lib.close()
            } else {
                grid.forceActiveFocus()
            }
        }
    }
}
