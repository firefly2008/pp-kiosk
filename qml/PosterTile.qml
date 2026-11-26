// Poster van een film of serie in het raster van de bibliotheek.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

Item {
    id: tile

    required property var modelData
    required property int index
    property bool selected: false
    property real fontSize: 14
    signal activated()
    signal hovered()

    z: selected ? 2 : 0

    Column {
        anchors.fill: parent
        anchors.margins: parent.width * 0.07
        spacing: parent.width * 0.04

        Rectangle {
            id: frame
            width: parent.width
            height: width * 1.5
            radius: width * 0.04
            color: Qt.hsla(((tile.modelData.title || "?").length * 41 % 360) / 360, 0.3, 0.24, 1)
            border.width: tile.selected ? Math.max(3, width * 0.03) : 0
            border.color: "white"
            scale: tile.selected ? 1.1 : 1.0
            Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }

            // Nog geen poster: titel in het vlak
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.centerIn: parent
                width: parent.width * 0.85
                visible: poster.status !== Image.Ready
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: "white"
                font.pixelSize: (tile.fontSize * 1.3) * appModel.fontScale
                font.weight: Font.DemiBold
                text: tile.modelData.title || ""
            }

            Image {
                id: poster
                anchors.fill: parent
                anchors.margins: frame.border.width
                source: tile.modelData.poster || ""
                sourceSize.width: 400
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                smooth: true
            }
        }

        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            color: tile.selected ? "white" : "#b0b4bb"
            font.pixelSize: (tile.fontSize) * appModel.fontScale
            text: tile.modelData.title + (tile.modelData.year ? " (" + tile.modelData.year + ")" : "")
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onEntered: tile.hovered()
        onClicked: tile.activated()
    }
}
