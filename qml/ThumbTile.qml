// Tegel voor de tabbladen-layout: plaatje (of logo op een gekleurd vlak) met de naam eronder.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

Item {
    id: tile

    // Ingevuld door de view vanuit het model
    required property int index
    required property string name
    required property string iconSource
    required property color accent

    property bool selected: false
    property string backdrop: ""
    property string subtitle: ""
    property real progress: -1
    property bool badge: false        // rood bolletje: er wordt opgenomen of er staat iets gepland
    readonly property bool isGlyph: iconSource.startsWith("glyph:")
    signal activated()
    signal menuRequested()
    signal hovered()

    z: selected ? 2 : 0

    readonly property real gap: width * 0.04

    Rectangle {
        id: card
        x: tile.gap
        y: tile.gap
        width: tile.width - tile.gap * 2
        height: width * 0.5625
        radius: height * 0.06
        color: tile.accent
        border.width: tile.selected ? Math.max(4, height * 0.035) : 0
        border.color: "white"
        scale: tile.selected ? 1.06 : 1.0
        Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#24ffffff" }
                GradientStop { position: 0.6; color: "#00ffffff" }
            }
        }

        Image {
            id: backdropImage
            anchors.fill: parent
            anchors.margins: card.border.width
            visible: tile.backdrop !== "" && status === Image.Ready
            source: tile.backdrop
            sourceSize.width: 640
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
        }

        // Geen plaatje: logo (of pictogram, of beginletter) in het midden
        Item {
            visible: !backdropImage.visible
            anchors.centerIn: parent
            width: card.height * 0.58
            height: width
            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                visible: !tile.isGlyph && (tile.iconSource === "" || icon.status === Image.Error)
                color: "white"
                font.pixelSize: parent.height * 0.8
                font.weight: Font.Bold
                text: tile.name.charAt(0).toUpperCase()
            }
            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                visible: tile.isGlyph
                color: "white"
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: parent.height * 0.8
                text: tile.isGlyph ? tile.iconSource.substring(6) : ""
            }
            Image {
                id: icon
                anchors.fill: parent
                sourceSize: Qt.size(256, 256)
                source: tile.isGlyph ? "" : tile.iconSource
                asynchronous: true
                fillMode: Image.PreserveAspectFit
                smooth: true
            }
        }

        // Voortgang (Verder kijken)
        Rectangle {
            visible: tile.progress >= 0
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: card.height * 0.06
            height: Math.max(3, card.height * 0.035)
            radius: height / 2
            color: "#55ffffff"
            Rectangle {
                width: parent.width * Math.max(0.02, Math.min(1, tile.progress))
                height: parent.height
                radius: height / 2
                color: "#ff5252"
            }
        }

        Rectangle {
            visible: tile.badge
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: card.height * 0.08
            width: card.height * 0.14
            height: width
            radius: width / 2
            color: "#e53935"
            border.color: "white"
            border.width: Math.max(1, width * 0.12)
        }
    }

    // Naam (en regel eronder) onder het plaatje
    Column {
        anchors.top: card.bottom
        anchors.topMargin: card.height * 0.1
        anchors.left: card.left
        anchors.right: card.right
        spacing: card.height * 0.02
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            width: parent.width
            color: tile.selected ? "white" : "#d5d8dc"
            font.pixelSize: (card.height * 0.13) * appModel.fontScale
            font.weight: tile.selected ? Font.DemiBold : Font.Normal
            elide: Text.ElideRight
            text: tile.name
        }
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            width: parent.width
            visible: text !== ""
            color: "#9fa4ab"
            font.pixelSize: (card.height * 0.1) * appModel.fontScale
            elide: Text.ElideRight
            text: tile.subtitle
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onEntered: tile.hovered()
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                tile.menuRequested()
            } else {
                tile.activated()
            }
        }
    }
}
