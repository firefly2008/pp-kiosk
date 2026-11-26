// Tegel "Verder kijken" (rij Media, naast Schilderij): films en afleveringen die je half hebt gezien.
// Enter = verder kijken, Menu = uit de lijst halen, Esc/B = terug.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

FocusScope {
    id: screen

    readonly property real unit: height / 100
    property Item returnFocusTo: null
    signal playRequested(int row)
    signal menuRequested(int row)

    anchors.fill: parent
    visible: false
    z: 55

    function open(focusItem) {
        returnFocusTo = focusItem
        grid.currentIndex = 0
        visible = true
        grid.forceActiveFocus()
        console.info("Resume screen:", resume.count)
    }

    function close() {
        visible = false
        if (returnFocusTo) returnFocusTo.forceActiveFocus()
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back || event.key === Qt.Key_Backspace) {
            close()
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#0b0c0f"
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }
    }

    Text {
        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
        id: title
        x: screen.unit * 5
        y: screen.unit * 4
        text: qsTr("Verder kijken")
        color: "white"
        font.pixelSize: screen.unit * 4.5 * appModel.fontScale
        font.weight: Font.DemiBold
    }

    Text {
        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
        anchors.left: title.left
        anchors.top: title.bottom
        anchors.topMargin: screen.unit * 3
        visible: resume.count === 0
        text: qsTr("Nog niets om verder te kijken.")
        color: "#a8acb3"
        font.pixelSize: screen.unit * 2.8 * appModel.fontScale
    }

    GridView {
        id: grid
        anchors.top: title.bottom
        anchors.topMargin: screen.unit * 2
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: hints.top
        anchors.bottomMargin: screen.unit * 2
        // Zelfde marges als het startscherm; ruimte rond elke tegel voor kader en vergroting
        anchors.leftMargin: screen.unit * 4
        anchors.rightMargin: screen.unit * 4
        cellWidth: Math.floor(width / 4)
        cellHeight: Math.floor(cellWidth * 0.66)
        clip: false
        keyNavigationEnabled: true
        highlightMoveDuration: 150
        model: resume

        delegate: Item {
            id: cell
            required property int index
            required property string name
            width: grid.cellWidth
            height: grid.cellHeight
            readonly property bool selected: grid.activeFocus && grid.currentIndex === index
            readonly property string sub: resume.subtitleAt(index + resume.revision * 0)
            // "S02E01 · Aflevering 1 · nog 41 min" → resterende tijd op een eigen regel
            // Laatste deel met de resterende tijd ("nog 3 min" / "3 min left") op een eigen regel
            readonly property var parts: sub.match(/^(.*?)\s+·\s+([^·]*\bmin\b[^·]*)$/)
            readonly property int split: parts ? 1 : -1
            z: selected ? 2 : 0

            Rectangle {
                id: card
                anchors.fill: parent
                anchors.margins: screen.unit * 1.4
                radius: screen.unit * 1.2
                color: "#1d2026"
                scale: cell.selected ? 1.06 : 1.0
                Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }

                Image {
                    anchors.fill: parent
                    source: resume.backdropAt(cell.index + resume.revision * 0) || resume.iconAt(cell.index)
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize.width: 640
                }
                // Donker verloop onderaan voor leesbare tekst
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: parent.height * 0.65
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#00000000" }
                        GradientStop { position: 1.0; color: "#e6000000" }
                    }
                }
                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: bar.top
                    anchors.margins: screen.unit * 1.4
                    anchors.bottomMargin: screen.unit * 0.9
                    spacing: screen.unit * 0.3
                    Text {
                        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                        width: parent.width
                        text: cell.name
                        color: "white"
                        elide: Text.ElideRight
                        font.pixelSize: screen.unit * 2.4 * appModel.fontScale
                        font.weight: Font.DemiBold
                    }
                    Text {
                        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                        width: parent.width
                        text: cell.parts ? cell.parts[1] : cell.sub
                        color: "#d4d7dc"
                        elide: Text.ElideRight
                        font.pixelSize: screen.unit * 1.8 * appModel.fontScale
                    }
                    Text {
                        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                        width: parent.width
                        visible: cell.split >= 0
                        text: cell.parts ? cell.parts[2] : ""
                        color: "#a8acb3"
                        font.pixelSize: screen.unit * 1.7 * appModel.fontScale
                    }
                }
                Rectangle {
                    id: bar
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: screen.unit * 1.4
                    height: screen.unit * 0.6
                    radius: height / 2
                    color: "#55ffffff"
                    Rectangle {
                        width: parent.width * Math.max(0.02, Math.min(1, resume.progressAt(cell.index + resume.revision * 0)))
                        height: parent.height
                        radius: height / 2
                        color: "#e5484d"
                    }
                }
                // Kader en gloed bij de gekozen tegel (zelfde maat als de kaart, valt niet over de buren)
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: "transparent"
                    border.width: cell.selected ? Math.max(3, screen.unit * 0.45) : 0
                    border.color: "white"
                }
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -screen.unit * 0.8
                    radius: parent.radius + screen.unit * 0.8
                    color: "transparent"
                    border.width: screen.unit * 0.8
                    border.color: "#59ffffff"
                    visible: cell.selected
                }
            }
            MouseArea {
                anchors.fill: card
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                hoverEnabled: true
                onEntered: { grid.currentIndex = cell.index; grid.forceActiveFocus() }
                onClicked: (mouse) => {
                    grid.currentIndex = cell.index
                    if (mouse.button === Qt.RightButton) screen.menuRequested(cell.index)
                    else screen.playRequested(cell.index)
                }
            }
        }

        Keys.onReturnPressed: screen.playRequested(currentIndex)
        Keys.onEnterPressed: screen.playRequested(currentIndex)
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Menu) {
                screen.menuRequested(currentIndex)
                event.accepted = true
            }
        }
    }

    HintBar {
        id: hints
        anchors.left: parent.left
        anchors.leftMargin: screen.unit * 5
        anchors.bottom: parent.bottom
        anchors.bottomMargin: screen.unit * 3
        unit: screen.unit
        items: [{ pad: "A", key: qsTr("Enter"), label: qsTr("Verder kijken") }, { pad: "X", key: qsTr("Menu"), label: qsTr("Uit de lijst") },
                { pad: "B", key: qsTr("Esc"), label: qsTr("Terug") }]
    }
}
