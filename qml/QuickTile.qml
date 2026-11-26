// Ronde snelknop in de bovenste rij (instellingen, wifi, afsluiten, ...).
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

Item {
    id: tile

    // Ingevuld door de view vanuit het model
    required property int index
    required property string label
    required property string glyph
    property bool selected: false
    // Te weinig ruimte: alleen het pictogram, de naam verschijnt bij focus
    property bool compact: false
    signal activated()
    signal hovered()

    Rectangle {
        anchors.fill: parent
        anchors.margins: 6
        radius: height / 2
        color: tile.selected ? "#e8eaed" : "#33ffffff"
        Behavior on color { ColorAnimation { duration: 120 } }

        Row {
            anchors.centerIn: parent
            spacing: parent.height * 0.25

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.verticalCenter: parent.verticalCenter
                color: tile.selected ? "#111318" : "white"
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: tile.height * 0.36
                text: tile.glyph
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.verticalCenter: parent.verticalCenter
                color: tile.selected ? "#111318" : "white"
                font.pixelSize: (tile.height * 0.34) * appModel.fontScale
                visible: !tile.compact
                text: tile.label
            }
        }
    }

    // Weinig ruimte: het woord verschijnt als label onder de gekozen knop
    Rectangle {
        visible: tile.compact && tile.selected
        z: 10
        anchors.top: parent.bottom
        anchors.topMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        width: bubbleText.implicitWidth + tile.height * 0.5
        height: tile.height * 0.62
        radius: height / 2
        color: "#e8eaed"
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            id: bubbleText
            anchors.centerIn: parent
            color: "#111318"
            font.pixelSize: tile.height * 0.32 * appModel.fontScale
            font.weight: Font.DemiBold
            text: tile.label
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onEntered: tile.hovered()
        onClicked: tile.activated()
    }
}
