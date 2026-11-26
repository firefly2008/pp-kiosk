// PP Kiosk -- startscherm (stap 1: basis na het opschonen van Kijkluister)
// Volgende stappen: dagstructuur-agenda, presentaties/afbeeldingen, snelkoppelingen, speler.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#F7F4EE"
    focus: true

    // Ontworpen voor 1920x1080; schaalt mee op andere schermen
    readonly property real s: Math.min(width / 1920, height / 1080)
    property date now: new Date()

    Timer {
        interval: 1000; running: true; repeat: true
        onTriggered: root.now = new Date()
    }

    Component.onCompleted: console.info("Home screen ready")

    // Bovenbalk: tijd en datum, altijd groot en leesbaar
    RowLayout {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 48 * root.s }
        height: 160 * root.s
        spacing: 32 * root.s

        Column {
            Layout.fillWidth: true
            Text {
                text: Qt.formatTime(root.now, "HH:mm")
                font.pixelSize: 110 * root.s; font.bold: true; color: "#1F2A44"
            }
            Text {
                text: root.now.toLocaleDateString(Qt.locale("nl_NL"), "dddd d MMMM")
                font.pixelSize: 44 * root.s; color: "#4A5571"
            }
        }
        Text {
            visible: weather.available
            text: weather.summary
            font.pixelSize: 44 * root.s; color: "#4A5571"
        }
    }

    // Hoofdtegels: groot genoeg voor een vinger op een 32 inch touchscreen
    GridLayout {
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom; margins: 48 * root.s }
        columns: 2
        rowSpacing: 32 * root.s
        columnSpacing: 32 * root.s

        Repeater {
            model: [
                { title: "Mijn dag", sub: "Dagplanning", tint: "#F2B33D" },
                { title: "Presentaties", sub: "PowerPoint en foto's", tint: "#5DA9E9" },
                { title: "Kijken en luisteren", sub: "Tv, radio, podcasts", tint: "#6CC28B" },
                { title: "Snelkoppelingen", sub: "NPO, RTL en meer", tint: "#E97A6B" }
            ]
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 28 * root.s
                color: tap.pressed ? Qt.darker(modelData.tint, 1.1) : modelData.tint

                Column {
                    anchors.centerIn: parent
                    spacing: 12 * root.s
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.title
                        font.pixelSize: 64 * root.s; font.bold: true; color: "#1F2A44"
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.sub
                        font.pixelSize: 36 * root.s; color: "#1F2A44"; opacity: 0.8
                    }
                }
                TapHandler {
                    id: tap
                    onTapped: console.info("Tile tapped: " + modelData.title)
                }
            }
        }
    }

    // Alleen voor de rooktest
    Timer {
        running: selfTest !== ""
        interval: 3000
        onTriggered: console.info("Self test " + selfTest + " ok")
    }

    Keys.onEscapePressed: if (selfTest !== "") Qt.quit()
}
