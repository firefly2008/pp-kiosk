// Brede tegel: logo met naam ernaast op een vlak in de kleur van het logo.
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
    // Verder kijken: groot plaatje, regel eronder en voortgangsbalk (anders leeg / -1)
    property string backdrop: ""
    property string subtitle: ""
    property real progress: -1
    property bool badge: false        // rood bolletje: er wordt opgenomen of er staat iets gepland
    readonly property bool wide: backdrop !== "" || progress >= 0
    // "glyph:<teken>": pictogram uit het Windows-lettertype in plaats van een afbeelding
    readonly property bool isGlyph: iconSource.startsWith("glyph:")
    signal activated()
    signal menuRequested()
    signal hovered()
    // Slepen: linkermuisknop ingedrukt houden en de tegel naar een andere plek bewegen
    property bool dragging: false
    signal dragMoved(real x, real y)

    // Gekozen tegel komt boven zijn buren te liggen
    z: dragging ? 3 : selected ? 2 : 0

    // Gloed in de kleur van de dienst rond de gekozen tegel (goed te zien vanaf de bank)
    Rectangle {
        anchors.fill: card
        anchors.margins: -tile.height * 0.07
        radius: card.radius * 1.6
        color: "transparent"
        border.width: tile.height * 0.07
        border.color: Qt.lighter(tile.accent, 1.6)
        opacity: tile.selected ? 0.55 : 0
        scale: card.scale
        Behavior on opacity { NumberAnimation { duration: 150 } }
    }

    Rectangle {
        id: card
        anchors.fill: parent
        anchors.margins: parent.height * 0.06
        radius: height * 0.06
        color: tile.accent
        border.width: tile.selected ? Math.max(4, height * 0.045) : 0
        border.color: "white"
        scale: tile.dragging ? 1.28 : tile.selected ? 1.2 : 1.0
        opacity: tile.dragging ? 0.85 : 1.0
        Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }

        // Lichte glans bovenin, zodat het vlak niet vlak oogt
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#24ffffff" }
                GradientStop { position: 0.6; color: "#00ffffff" }
            }
        }

        // Verder kijken: plaatje over de hele tegel, titel en voortgang onderin
        Image {
            id: backdropImage
            anchors.fill: parent
            anchors.margins: card.border.width
            visible: tile.wide && status === Image.Ready
            source: tile.backdrop
            sourceSize.width: 640
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
        }
        Rectangle {
            visible: tile.wide
            anchors.fill: parent
            anchors.margins: card.border.width
            radius: card.radius
            gradient: Gradient {
                GradientStop { position: 0.35; color: "#00000000" }
                GradientStop { position: 1.0; color: "#e0000000" }
            }
        }
        Column {
            visible: tile.wide
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: card.height * 0.08
            spacing: card.height * 0.03
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                color: "white"
                font.pixelSize: (card.height * 0.15) * appModel.fontScale
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                text: tile.name
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                visible: text !== ""
                color: "#d5d8dc"
                font.pixelSize: (card.height * 0.11) * appModel.fontScale
                elide: Text.ElideRight
                text: tile.subtitle
            }
            Rectangle {
                visible: tile.progress >= 0
                width: parent.width
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
        }

        Row {
            id: content
            visible: !tile.wide
            anchors.centerIn: parent
            spacing: card.height * 0.1

            Item {
                width: card.height * 0.56
                height: width
                anchors.verticalCenter: parent.verticalCenter

                // Nog geen pictogram: beginletter
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    anchors.centerIn: parent
                    visible: !tile.isGlyph && (tile.iconSource === "" || icon.status === Image.Error)
                    color: "white"
                    font.pixelSize: parent.height * 0.8
                    font.weight: Font.Bold
                    text: tile.name.charAt(0).toUpperCase()
                }
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
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

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, card.width - card.height * 0.56 - card.height * 0.34)
                color: "white"
                font.pixelSize: (card.height * 0.2) * appModel.fontScale
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                text: tile.name
            }
        }
    }

    Rectangle {
        visible: tile.badge
        z: 4
        anchors.top: card.top
        anchors.right: card.right
        anchors.margins: card.height * 0.08
        width: card.height * 0.16
        height: width
        radius: width / 2
        color: "#e53935"
        border.color: "white"
        border.width: Math.max(1, width * 0.12)
        scale: card.scale
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        // Tijdens het slepen mag de lijst de muis niet overnemen om te scrollen
        preventStealing: tile.dragging
        onEntered: tile.hovered()
        onClicked: (mouse) => {
            if (mouse.wasHeld) {
                return
            }
            if (mouse.button === Qt.RightButton) {
                tile.menuRequested()
            } else {
                tile.activated()
            }
        }
        onPressAndHold: (mouse) => {
            if (mouse.button === Qt.LeftButton) {
                tile.dragging = true
            }
        }
        onPositionChanged: (mouse) => {
            if (tile.dragging) {
                tile.dragMoved(mouse.x, mouse.y)
            }
        }
        onReleased: tile.dragging = false
        onCanceled: tile.dragging = false
    }
}
