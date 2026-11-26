// Presentaties: kiezen uit de lijst, daarna dia's schermvullend (automatisch verder, vegen of tikken).
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: page
    color: "#F7F4EE"
    required property real s
    signal back()

    readonly property color ink: "#1F2A44"
    property var show: null          // gekozen presentatie (item uit presentations.list)
    property var slides: []
    property int slide: 0
    property bool paused: false

    function start(item) {
        slides = presentations.slides(item.folder)
        slide = 0
        show = item
        paused = false
        console.info("Presentation started: " + item.title + " slides " + slides.length)
    }
    function stop() { show = null; slides = [] }
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
                color: backTap.pressed ? "#DDE3EA" : "#FFFFFF"
                border.color: "#14000000"; border.width: 2
                Text { anchors.centerIn: parent; text: "‹  Terug"; font.pixelSize: 40 * page.s; font.bold: true; color: page.ink }
                TapHandler { id: backTap; onTapped: page.back() }
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
                    color: "#FFFFFF"
                    clip: true
                    Image {
                        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: label.top }
                        source: modelData.cover
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        sourceSize.width: 800
                    }
                    Rectangle {
                        id: label
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                        height: 100 * page.s
                        color: openTap.pressed ? "#E6F3EC" : "#FFFFFF"
                        Text {
                            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 24 * page.s }
                            text: modelData.title + "  ·  " + modelData.count + (modelData.count === 1 ? " dia" : " dia's")
                            font.pixelSize: 36 * page.s; font.bold: true; color: page.ink
                            elide: Text.ElideRight
                        }
                    }
                    TapHandler { id: openTap; onTapped: page.start(modelData) }
                }
            }
            Text {
                anchors.centerIn: parent
                visible: grid.count === 0
                text: "Nog geen presentaties. Voeg ze toe via de beheerpagina."
                font.pixelSize: 40 * page.s; color: "#5B6478"
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
                Behavior on opacity { NumberAnimation { duration: 700 } }

                Image {
                    anchors.fill: parent
                    source: modelData.source
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize.width: 1920
                }
                // Tekst op de dia (titel, ondertitel, label rechtsboven)
                Rectangle {
                    visible: modelData.title !== ""
                    anchors { left: parent.left; bottom: parent.bottom; leftMargin: 72 * page.s; bottomMargin: 96 * page.s }
                    width: Math.max(captionCol.implicitWidth + 96 * page.s, 520 * page.s)
                    height: captionCol.implicitHeight + 64 * page.s
                    radius: 36 * page.s
                    color: "#E61F2A44"
                    Column {
                        id: captionCol
                        anchors.centerIn: parent
                        spacing: 8 * page.s
                        Text { text: modelData.title; font.pixelSize: 88 * page.s; font.bold: true; color: "#FFC72C" }
                        Text { visible: text !== ""; text: modelData.subtitle; font.pixelSize: 56 * page.s; font.bold: true; color: "#FFFFFF" }
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

        // Tikken links/rechts = vorige/volgende, vegen ook; midden = bediening tonen
        TapHandler {
            onTapped: (point) => {
                const x = point.position.x
                if (x < parent.width * 0.25) page.step(-1)
                else if (x > parent.width * 0.75) page.step(1)
                else controls.shown = !controls.shown
            }
        }
        DragHandler {
            target: null
            xAxis.enabled: true; yAxis.enabled: false
            onActiveChanged: if (!active && Math.abs(centroid.position.x - centroid.pressPosition.x) > 120 * page.s)
                                 page.step(centroid.position.x < centroid.pressPosition.x ? 1 : -1)
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
