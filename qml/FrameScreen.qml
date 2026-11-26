// Schilderij-modus: kunst of natuurfoto's schermvullend, als een schilderij aan de muur.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

FocusScope {
    id: frameScreen

    readonly property real unit: height / 100
    property Item returnFocusTo: null
    property bool front: true          // welke van de twee lagen nu zichtbaar is
    readonly property var item: frame.current

    anchors.fill: parent
    visible: false
    z: 60

    // Zolang het schilderij openstaat de toetsen vasthouden (ook als iets anders focus probeerde te pakken)
    onActiveFocusChanged: {
        if (visible && !activeFocus) {
            Qt.callLater(() => { if (frameScreen.visible && !frameScreen.activeFocus) frameScreen.forceActiveFocus() })
        }
    }

    function open(focusItem) {
        returnFocusTo = focusItem
        visible = true
        frame.keepAwake(true)
        frame.start()
        if (item && item.image) {
            show(item)
        }
        forceActiveFocus()
        flashInfo()
    }

    function close() {
        console.info("Frame closed")
        settings.visible = false
        frame.keepAwake(false)
        visible = false
        if (returnFocusTo) {
            returnFocusTo.forceActiveFocus()
        }
    }

    function flashInfo() {
        infoTimer.restart()
    }

    // Nieuwe afbeelding eerst onzichtbaar laden; pas als hij klaar is overvloeien
    function show(it) {
        const target = front ? layerB : layerA
        target.pending = it
        target.image.source = it.image
    }

    Connections {
        target: frame
        function onCurrentChanged() {
            if (frameScreen.visible && frame.current.image) {
                frameScreen.show(frame.current)
            }
        }
    }

    Timer {
        id: nextTimer
        interval: frame.interval * 60000
        running: frameScreen.visible && !settings.visible
        repeat: true
        onTriggered: frame.next()
    }
    Timer {
        id: infoTimer
        interval: 8000
    }

    Rectangle {
        anchors.fill: parent
        color: "#141416"
    }

    // Twee lagen voor het overvloeien
    component Layer: Item {
        id: layer
        property var pending: null
        property var shown: null
        property alias image: img
        anchors.fill: parent
        opacity: 0
        Behavior on opacity { NumberAnimation { duration: 1800; easing.type: Easing.InOutQuad } }

        // Passe-partout en lijst (in de stand "als schilderij")
        Rectangle {
            id: mat
            visible: !frame.fill && img.status === Image.Ready
            anchors.centerIn: parent
            width: img.paintedWidth + frameScreen.unit * 9
            height: img.paintedHeight + frameScreen.unit * 9
            color: "#f4f1ea"
            border.color: "#2b2118"
            border.width: frameScreen.unit * 1.4
        }
        Image {
            id: img
            anchors.centerIn: parent
            width: frame.fill ? parent.width : parent.width * 0.78
            height: frame.fill ? parent.height : parent.height * 0.74
            fillMode: frame.fill ? Image.PreserveAspectCrop : Image.PreserveAspectFit
            sourceSize.width: 2560
            asynchronous: true
            cache: false
            smooth: true
            onStatusChanged: {
                if (status === Image.Ready && layer.pending) {
                    layer.shown = layer.pending
                    layer.pending = null
                    frameScreen.front = layer === layerA
                    if (frame.showInfo) {
                        frameScreen.flashInfo()
                    }
                } else if (status === Image.Error && layer.pending) {
                    console.warn("Frame: image failed", source)
                    layer.pending = null
                    frame.next()   // deze overslaan
                }
            }
        }
    }
    Layer { id: layerA; opacity: frameScreen.front && image.status === Image.Ready ? 1 : 0 }
    Layer { id: layerB; opacity: !frameScreen.front && image.status === Image.Ready ? 1 : 0 }

    readonly property var shownItem: front ? layerA.shown : layerB.shown

    // Titel, maker en bron
    Rectangle {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: frameScreen.unit * 4
        width: Math.min(parent.width * 0.6, info.implicitWidth + frameScreen.unit * 5)
        height: info.implicitHeight + frameScreen.unit * 3.5
        radius: frameScreen.unit * 1.2
        color: "#b0000000"
        visible: !!frameScreen.shownItem
        opacity: infoTimer.running || settings.visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 600 } }
        Column {
            id: info
            x: frameScreen.unit * 2.5
            anchors.verticalCenter: parent.verticalCenter
            width: frameScreen.width * 0.6 - frameScreen.unit * 5
            spacing: frameScreen.unit * 0.5
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: Math.min(implicitWidth, parent.width)
                elide: Text.ElideRight
                color: "white"
                font.pixelSize: (frameScreen.unit * 3) * appModel.fontScale
                font.weight: Font.DemiBold
                text: frameScreen.shownItem ? frameScreen.shownItem.title : ""
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: Math.min(implicitWidth, parent.width)
                elide: Text.ElideRight
                color: "#d5d8dc"
                font.pixelSize: (frameScreen.unit * 2.2) * appModel.fontScale
                visible: text !== ""
                text: frameScreen.shownItem ? frameScreen.shownItem.subtitle : ""
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                color: "#9a9ea6"
                font.pixelSize: (frameScreen.unit * 1.8) * appModel.fontScale
                text: (frameScreen.shownItem ? frameScreen.shownItem.credit : "")
                      + (gamepad.connectedCount > 0 ? qsTr("    ·    ◀ ▶ bladeren   ·   Ⓐ instellingen   ·   Ⓑ stoppen")
                                                    : qsTr("    ·    ← → bladeren   ·   Enter: instellingen   ·   Esc of rechtermuisknop: stoppen"))
            }
        }
    }

    // Bezig of foutmelding
    Text {
        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
        anchors.centerIn: parent
        visible: !frameScreen.shownItem
        color: "#b0b4bb"
        font.pixelSize: (frameScreen.unit * 3) * appModel.fontScale
        text: frame.status !== "" ? frame.status : qsTr("Afbeeldingen zoeken…")
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPositionChanged: frameScreen.flashInfo()
        // Links: instellingen, rechts: stoppen
        onClicked: (mouse) => mouse.button === Qt.RightButton ? frameScreen.close() : settings.open()
    }

    Keys.onLeftPressed: { frame.previous(); flashInfo() }
    Keys.onRightPressed: { frame.next(); flashInfo() }
    Keys.onUpPressed: flashInfo()
    Keys.onDownPressed: flashInfo()
    Keys.onReturnPressed: settings.open()
    Keys.onEnterPressed: settings.open()
    Keys.onMenuPressed: settings.open()
    Keys.onEscapePressed: close()
    Keys.onBackPressed: close()
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_I) {
            flashInfo()
            event.accepted = true
        }
    }

    // ------------------------------------------------------------------
    // Instellingen
    // ------------------------------------------------------------------
    FocusScope {
        id: settings
        anchors.fill: parent
        visible: false
        z: 10

        readonly property var sourceLabels: [qsTr("Schilderijen"), qsTr("Topstukken (The Met)"), qsTr("Natuurfoto's"), qsTr("Natuurfoto's (Unsplash)"), qsTr("Afwisselend")]
        readonly property var intervals: [1, 5, 15, 30, 60]

        function open() {
            keyField.text = frame.pexelsKey
            visible = true
            sourceRow.currentIndex = frame.source
            sourceRow.forceActiveFocus()
        }
        function close() {
            if (keyField.text !== frame.pexelsKey) {
                frame.pexelsKey = keyField.text
            }
            visible = false
            frameScreen.forceActiveFocus()
        }

        Keys.onEscapePressed: close()
        Keys.onBackPressed: close()
        Keys.onMenuPressed: close()

        Rectangle {
            anchors.fill: parent
            color: "#cc000000"
            MouseArea { anchors.fill: parent; onClicked: settings.close() }
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(frameScreen.width * 0.9, frameScreen.unit * 170)
            height: panel.implicitHeight + frameScreen.unit * 8
            radius: frameScreen.unit * 2
            color: "#202227"
            MouseArea { anchors.fill: parent }

            // Rij met keuzeknoppen (pijltjes links/rechts, OK kiest)
            component OptionRow: FocusScope {
                id: row
                property var labels: []
                property int chosen: -1
                property int currentIndex: 0
                signal picked(int index)
                width: parent.width
                height: flow.implicitHeight
                onActiveFocusChanged: if (activeFocus) buttons.itemAt(Math.max(0, currentIndex)).forceActiveFocus()
                Keys.onLeftPressed: currentIndex = Math.max(0, currentIndex - 1)
                Keys.onRightPressed: currentIndex = Math.min(labels.length - 1, currentIndex + 1)
                onCurrentIndexChanged: if (activeFocus && buttons.itemAt(currentIndex)) buttons.itemAt(currentIndex).forceActiveFocus()
                // Lange rijen lopen door op de volgende regel
                Flow {
                    id: flow
                    width: parent.width
                    spacing: frameScreen.unit * 1.2
                    Repeater {
                        id: buttons
                        model: row.labels
                        delegate: FocusButton {
                            required property string modelData
                            required property int index
                            unit: frameScreen.unit
                            text: modelData
                            checked: index === row.chosen
                            onActiveFocusChanged: if (activeFocus) row.currentIndex = index
                            onClicked: {
                                row.currentIndex = index
                                row.picked(index)
                            }
                        }
                    }
                }
            }
            component Label: Text {
                color: "#9fb4d0"
                font.pixelSize: (frameScreen.unit * 2.2) * appModel.fontScale
            }

            Column {
                id: panel
                x: frameScreen.unit * 4
                y: frameScreen.unit * 4
                width: parent.width - frameScreen.unit * 8
                spacing: frameScreen.unit * 1.4

                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    color: "white"
                    font.pixelSize: (frameScreen.unit * 4) * appModel.fontScale
                    font.weight: Font.DemiBold
                    text: qsTr("Schilderij")
                }
                Label { text: qsTr("Wat wil je zien?") }
                OptionRow {
                    id: sourceRow
                    labels: settings.sourceLabels
                    chosen: frame.source
                    onPicked: (i) => frame.source = i
                    KeyNavigation.down: intervalRow
                }
                Label { text: qsTr("Wisselen elke") }
                OptionRow {
                    id: intervalRow
                    labels: settings.intervals.map(m => m + qsTr(" min"))
                    chosen: settings.intervals.indexOf(frame.interval)
                    onPicked: (i) => frame.interval = settings.intervals[i]
                    KeyNavigation.up: sourceRow
                    KeyNavigation.down: styleRow
                }
                Label { text: qsTr("Weergave") }
                OptionRow {
                    id: styleRow
                    labels: [qsTr("Schermvullend"), qsTr("Als schilderij in een lijst")]
                    chosen: frame.fill ? 0 : 1
                    onPicked: (i) => frame.fill = i === 0
                    KeyNavigation.up: intervalRow
                    KeyNavigation.down: infoRow
                }
                Label { text: qsTr("Titel en maker tonen") }
                OptionRow {
                    id: infoRow
                    labels: [qsTr("Bij elke nieuwe afbeelding"), qsTr("Alleen als ik op een knop druk")]
                    chosen: frame.showInfo ? 0 : 1
                    onPicked: (i) => frame.showInfo = i === 0
                    KeyNavigation.up: styleRow
                    KeyNavigation.down: closeButton
                }
                // Unsplash heeft een ingebouwde sleutel: geen eigen sleutel meer nodig
                Rectangle {
                    visible: false
                    width: parent.width
                    height: frameScreen.unit * 6
                    radius: frameScreen.unit
                    color: "#0f1115"
                    border.width: keyField.activeFocus ? 3 : 1
                    border.color: keyField.activeFocus ? "#e8eaed" : "#33ffffff"
                    TextInput {
                        id: keyField
                        anchors.fill: parent
                        anchors.margins: frameScreen.unit * 1.5
                        verticalAlignment: TextInput.AlignVCenter
                        color: "white"
                        font.pixelSize: (frameScreen.unit * 2.3) * appModel.fontScale
                        clip: true
                        selectByMouse: true
                        echoMode: activeFocus ? TextInput.Normal : TextInput.Password
                        KeyNavigation.up: infoRow
                        KeyNavigation.down: closeButton
                        Keys.onReturnPressed: frame.pexelsKey = text
                    }
                }
                Row {
                    spacing: frameScreen.unit * 1.2
                    FocusButton {
                        id: closeButton
                        unit: frameScreen.unit
                        text: qsTr("Klaar")
                        onClicked: settings.close()
                        KeyNavigation.up: infoRow
                        KeyNavigation.right: stopButton
                    }
                    FocusButton {
                        id: stopButton
                        unit: frameScreen.unit
                        text: qsTr("Schilderij stoppen")
                        onClicked: frameScreen.close()
                        KeyNavigation.up: infoRow
                        KeyNavigation.left: closeButton
                    }
                }
            }
        }
    }

    // Test: instellingen openen
    function openSettings() {
        settings.open()
    }
}
