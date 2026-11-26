// Zwevende Home-knop linksboven, boven een website of app (in plaats van de grijze titelbalk).
// Verschijnt na het openen en zodra de muis bovenaan het scherm komt.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Window

Window {
    id: homeWindow

    readonly property real unit: Screen.height / 100
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    color: "transparent"
    x: Screen.virtualX + unit * 1.5
    y: Screen.virtualY + unit * 1.2
    width: pill.width
    height: pill.height
    visible: homeBridge.showButton

    Rectangle {
        id: pill
        width: row.width + homeWindow.unit * 3.2
        height: homeWindow.unit * 5.6
        radius: height / 2
        color: mouse.containsMouse ? "#e63d6fb4" : "#d916181d"
        border.color: "#66ffffff"
        border.width: 1

        Row {
            id: row
            anchors.centerIn: parent
            spacing: homeWindow.unit * 1.1
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.verticalCenter: parent.verticalCenter
                text: ""   // huisje
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: homeWindow.unit * 2.6
                color: "white"
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Kijkluister")
                font.pixelSize: homeWindow.unit * 2.2
                font.weight: Font.DemiBold
                color: "white"
            }
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: homeBridge.goHome()
        }
    }
}
