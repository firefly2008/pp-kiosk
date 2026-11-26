// Knop die met pijltjes/gamepad bereikbaar is (Enter of A = klikken).
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

Rectangle {
    id: button

    property string text
    property real unit: 10
    // Gekozen optie in een rij keuzeknoppen
    property bool checked: false
    signal clicked()

    activeFocusOnTab: true
    height: unit * 6
    width: label.implicitWidth + unit * 5
    radius: height / 2
    color: activeFocus ? "#e8eaed" : checked ? "#3d6fb4" : (mouse.containsMouse ? "#55ffffff" : "#33ffffff")

    Text {
        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
        id: label
        anchors.centerIn: parent
        color: button.activeFocus ? "#111318" : "white"
        font.pixelSize: (button.unit * 2.4) * appModel.fontScale
        font.weight: Font.DemiBold
        text: button.text
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            button.forceActiveFocus()
            button.clicked()
        }
    }

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
    Keys.onSpacePressed: clicked()
}
