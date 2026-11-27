// Presentaties: kiezen uit de lijst, daarna dia's schermvullend (automatisch verder, vegen of tikken).
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: page
    color: glass ? "#0A0F1E" : "#F7F4EE"
    required property real s
    signal back()

    // Glas-thema (gekozen op de beheerpagina): donker met gloed en doorschijnende kaarten
    readonly property bool glass: agenda.settings.theme === "glas" && agenda.settings.contrast !== true
    readonly property real glassAlpha: 1 - Math.max(50, Math.min(95, agenda.settings.glassTransparency || 80)) / 100
    readonly property color cardBg: glass ? Qt.rgba(1, 1, 1, glassAlpha) : "#FFFFFF"
    readonly property color pressBg: glass ? "#40FFFFFF" : "#DDE3EA"
    readonly property color lineColor: glass ? "#40FFFFFF" : "#14000000"
    GlassBackground { anchors.fill: parent; visible: page.glass }

    readonly property color soft: glass ? "#C3CAE0" : "#5B6478"
    readonly property color ink: glass ? "#FFFFFF" : "#1F2A44"
    property var show: null          // gekozen presentatie (item uit presentations.list)
    property var slides: []
    property int slide: 0
    property bool paused: false
    // Screensaver als fotoalbum: zacht in elkaar overvloeien en langzaam inzoomen (beheer: Screensaver)
    property bool kenBurns: false

    function start(item) {
        slides = presentations.slides(item.folder)
        slide = 0
        show = item
        paused = false
        console.info("Presentation started: " + item.title + " slides " + slides.length)
    }
    function stop() { show = null; slides = [] }
    // Map veranderd terwijl de presentatie loopt (bijvoorbeeld nieuw menu ingevuld): nieuwe dia's laden
    Connections {
        target: presentations
        function onChanged() {
            if (!page.show) return
            const now = presentations.slides(page.show.folder)
            if (now.length === 0) { page.stop(); return }
            page.slides = now
            if (page.slide >= now.length) page.slide = 0
            console.info("Presentation reloaded: " + page.show.title + " slides " + now.length)
        }
    }
    // Screensaver: presentatie op naam starten (leeg of onbekend: de eerste)
    function startNamed(name) {
        const list = presentations.list
        if (list.length === 0) return false
        let pick = list.find(x => x.title === name || x.folder === name)
        start(pick || list[0])
        return true
    }
    function step(d) {
        if (slides.length === 0) return
        slide = (slide + d + slides.length) % slides.length
        advance.restart()
    }

    // ---------- Overzicht ----------
    Item {
        anchors.fill: parent
        visible: page.show === null

        RowLayout {
            id: top
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 40 * page.s }
            height: 120 * page.s
            spacing: 24 * page.s
            Rectangle {
                Layout.preferredWidth: 220 * page.s; Layout.preferredHeight: 100 * page.s
                radius: 24 * page.s
                color: backTap.pressed ? page.pressBg : page.cardBg
                border.color: page.lineColor; border.width: 2
                Text { anchors.centerIn: parent; text: "‹  Terug"; font.pixelSize: 40 * page.s; font.bold: true; color: page.ink }
                TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds; id: backTap; onTapped: page.back() }
            }
            Text {
                Layout.fillWidth: true
                text: "🖼️  Presentaties"
                font.pixelSize: 64 * page.s; font.bold: true; color: page.ink
            }
        }

        GridView {
            id: grid
            anchors { left: parent.left; right: parent.right; top: top.bottom; bottom: parent.bottom; margins: 40 * page.s; topMargin: 24 * page.s }
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            cellWidth: width / 3
            cellHeight: cellWidth * 0.66
            model: presentations.list
            delegate: Item {
                required property var modelData
                width: grid.cellWidth; height: grid.cellHeight
                Rectangle {
                    anchors { fill: parent; margins: 14 * page.s }
                    radius: 28 * page.s
                    color: page.cardBg
                    clip: true
                    Image {
                        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: label.top }
                        source: modelData.menu && agenda.todayMenu.photoUrl ? agenda.todayMenu.photoUrl : modelData.cover
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        sourceSize.width: 800
                    }
                    Rectangle {
                        id: label
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                        height: 100 * page.s
                        color: openTap.pressed ? (page.glass ? "#40FFFFFF" : "#E6F3EC") : page.cardBg
                        Text {
                            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 24 * page.s }
                            text: modelData.title + "  ·  " + modelData.count + (modelData.count === 1 ? " dia" : " dia's")
                            font.pixelSize: 36 * page.s; font.bold: true; color: page.ink
                            elide: Text.ElideRight
                        }
                    }
                    TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds; id: openTap; onTapped: page.start(modelData) }
                }
            }
            Text {
                anchors.centerIn: parent
                visible: grid.count === 0
                text: "Nog geen presentaties. Voeg ze toe via de beheerpagina."
                font.pixelSize: 40 * page.s; color: page.soft
            }
        }
    }

    // ---------- Diavoorstelling ----------
    Rectangle {
        anchors.fill: parent
        visible: page.show !== null
        color: "#000000"

        Repeater {
            model: page.slides
            delegate: Item {
                required property var modelData
                required property int index
                anchors.fill: parent
                opacity: index === page.slide ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: page.kenBurns ? 1800 : 700 } }
                clip: true

                Image {
                    id: slideImage
                    anchors.fill: parent
                    transformOrigin: index % 3 === 0 ? Item.TopLeft : index % 3 === 1 ? Item.BottomRight : Item.Center
                    NumberAnimation on scale {
                        running: page.kenBurns && index === page.slide
                        from: 1.0; to: 1.12
                        duration: (page.show ? page.show.interval : 10) * 1000 + 2000
                    }
                    // Diner van de dag: foto die bij het eten van vandaag past (eigen foto of een passende standaardfoto)
                    source: parent.fromMenu && agenda.todayMenu.photoUrl ? agenda.todayMenu.photoUrl : modelData.source
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize.width: 1920
                }
                // Weekmenu: ondertitel = eten van vandaag (bij een presentatie met "menu")
                readonly property bool fromMenu: page.show && page.show.menu && agenda.todayMenu.dish
                readonly property string capTitle: fromMenu && !modelData.title ? "Diner van de dag" : modelData.title
                readonly property string capSub: fromMenu ? (agenda.todayMenu.dish + (agenda.todayMenu.note ? " " + agenda.todayMenu.note : "")) : modelData.subtitle
                // Tekst op de dia (titel, ondertitel, label rechtsboven)
                Rectangle {
                    visible: parent.capTitle !== ""
                    anchors { left: parent.left; bottom: parent.bottom; leftMargin: 72 * page.s; bottomMargin: 96 * page.s }
                    width: Math.max(captionCol.implicitWidth + 96 * page.s, 520 * page.s)
                    height: captionCol.implicitHeight + 64 * page.s
                    radius: 36 * page.s
                    color: "#E61F2A44"
                    Column {
                        id: captionCol
                        anchors.centerIn: parent
                        spacing: 8 * page.s
                        Text { text: captionCol.parent.parent.capTitle; font.pixelSize: 88 * page.s; font.bold: true; color: "#FFC72C" }
                        Text { visible: text !== ""; text: captionCol.parent.parent.capSub; font.pixelSize: 56 * page.s; font.bold: true; color: "#FFFFFF" }
                    }
                }
                Rectangle {
                    visible: modelData.badge !== ""
                    anchors { right: parent.right; top: parent.top; margins: 64 * page.s }
                    width: 240 * page.s; height: 240 * page.s; radius: width / 2
                    color: "#DA291C"
                    rotation: -8
                    Column {
                        anchors.centerIn: parent
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "vanaf"; font.pixelSize: 34 * page.s; color: "#FFFFFF" }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.badge; font.pixelSize: 68 * page.s; font.bold: true; color: "#FFFFFF" }
                    }
                }
            }
        }

        Timer {
            id: advance
            interval: (page.show ? page.show.interval : 10) * 1000
            running: page.show !== null && !page.paused && page.slides.length > 1
            repeat: true
            onTriggered: page.step(1)
        }

        // Tikken links/rechts = vorige/volgende, vegen ook; midden = bediening tonen.
        // Een MouseArea vangt de tik helemaal op: de lijst met presentaties en het startscherm
        // eronder reageren dan niet mee (eerder startte een tik soms een andere presentatie).
        MouseArea {
            anchors.fill: parent
            property real pressX: 0
            onPressed: (mouse) => pressX = mouse.x
            onReleased: (mouse) => {
                const dx = mouse.x - pressX
                if (Math.abs(dx) > 120 * page.s) page.step(dx < 0 ? 1 : -1)
                else if (mouse.x < width * 0.25) page.step(-1)
                else if (mouse.x > width * 0.75) page.step(1)
                else controls.shown = !controls.shown
            }
        }

        Rectangle {
            id: controls
            property bool shown: true
            onShownChanged: if (shown) hide.restart()
            Component.onCompleted: hide.restart()
            Timer { id: hide; interval: 5000; onTriggered: controls.shown = false }
            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 24 * page.s }
            width: ctlRow.implicitWidth + 32 * page.s; height: 120 * page.s
            radius: 30 * page.s
            color: "#CC000000"
            opacity: shown ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 250 } }
            Row {
                id: ctlRow
                anchors.centerIn: parent
                spacing: 16 * page.s
                Repeater {
                    model: [
                        { label: "‹  Stoppen", act: "stop" },
                        { label: "◀", act: "prev" },
                        { label: page.paused ? "▶" : "❚❚", act: "pause" },
                        { label: "▶|", act: "next" }
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        width: (modelData.act === "stop" ? 260 : 120) * page.s; height: 96 * page.s
                        radius: 22 * page.s
                        color: ctlTap.pressed ? "#55FFFFFF" : "#22FFFFFF"
                        Text { anchors.centerIn: parent; text: modelData.label; font.pixelSize: 40 * page.s; font.bold: true; color: "#FFFFFF" }
                        TapHandler {
                            gesturePolicy: TapHandler.ReleaseWithinBounds   // knop: tik gaat niet door naar wat eronder ligt
                            id: ctlTap
                            onTapped: {
                                hide.restart()
                                if (modelData.act === "stop") page.stop()
                                else if (modelData.act === "prev") page.step(-1)
                                else if (modelData.act === "next") page.step(1)
                                else page.paused = !page.paused
                            }
                        }
                    }
                }
            }
        }
        Text {
            anchors { right: parent.right; bottom: parent.bottom; margins: 32 * page.s }
            visible: page.slides.length > 1
            text: (page.slide + 1) + " / " + page.slides.length
            font.pixelSize: 32 * page.s; color: "#FFFFFF"; style: Text.Outline; styleColor: "#80000000"
        }
    }
}
