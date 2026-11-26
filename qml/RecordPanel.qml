// Opnemen van live tv: map kiezen, duur kiezen, lopende opname volgen en
// eerdere opnames terugkijken. Het opnemen zelf doet ffmpeg (zie recorder.cpp).
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Dialogs

FocusScope {
    id: panel

    property real unit: height / 100
    property var channel: ({})
    property Item returnFocusTo: null
    property int confirmDelete: -1
    readonly property var durations: [0, 30, 60, 90, 120, 180]
    property int durationIndex: 2

    anchors.fill: parent
    visible: false
    z: 45

    function open(ch, focusItem) {
        channel = ch || {}
        returnFocusTo = focusItem
        confirmDelete = -1
        message.text = ""
        if (recorder.folder === "") {
            recorder.folder = recorder.defaultFolder()
        }
        recorder.refresh()
        visible = true
        if (recorder.recording) {
            stopButton.forceActiveFocus()
        } else {
            startButton.forceActiveFocus()
        }
    }

    function close() {
        visible = false
        if (returnFocusTo) {
            returnFocusTo.forceActiveFocus()
        }
    }

    function fmtTime(s) {
        if (s < 0) return ""
        const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60), sec = s % 60
        return (h > 0 ? h + ":" : "") + (m < 10 && h > 0 ? "0" : "") + m + ":" + (sec < 10 ? "0" : "") + sec
    }
    function fmtSize(b) {
        return b > 1e9 ? (b / 1e9).toFixed(2) + " GB" : b > 1e6 ? Math.round(b / 1e6) + " MB" : Math.round(b / 1e3) + " kB"
    }

    function startRecording() {
        if (!channel.url) {
            message.text = qsTr("Geen zender gekozen")
            return
        }
        if (recorder.start(channel.name, channel.url, durations[durationIndex])) {
            message.text = ""
            stopButton.forceActiveFocus()
        } else {
            message.text = recorder.status
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Map voor opnames kiezen")
        onAccepted: recorder.folder = recorder.localPath(selectedFolder)
    }

    Keys.onEscapePressed: close()
    Keys.onBackPressed: close()
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_R) {
            close()
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#0b0c0f"
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; hoverEnabled: true }
    }

    Column {
        id: content
        x: panel.unit * 6
        y: panel.unit * 5
        width: parent.width - panel.unit * 12
        spacing: panel.unit * 1.6

        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            color: "white"
            font.pixelSize: (panel.unit * 4) * appModel.fontScale
            font.weight: Font.DemiBold
            text: qsTr("Opnemen  ·  ") + (panel.channel.name || "")
        }
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            color: "#a8acb3"
            font.pixelSize: (panel.unit * 2) * appModel.fontScale
            text: qsTr("De opname loopt door op de achtergrond, ook als je een andere zender kijkt.") + (gamepad.connectedCount > 0 ? qsTr("   Ⓑ sluiten") : qsTr("   Esc: sluiten"))
        }

        // Map
        Text { color: "#9fb4d0"; font.pixelSize: (panel.unit * 2.2) * appModel.fontScale; text: qsTr("Opslaan in")}
        Row {
            spacing: panel.unit * 1.5
            Rectangle {
                width: content.width - folderButton.width - openButton.width - panel.unit * 3
                height: panel.unit * 6
                radius: panel.unit
                color: "#0f1115"
                border.width: 1
                border.color: "#33ffffff"
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    anchors.fill: parent
                    anchors.margins: panel.unit * 1.5
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideMiddle
                    color: "white"
                    font.pixelSize: (panel.unit * 2.3) * appModel.fontScale
                    text: recorder.folder
                }
            }
            FocusButton {
                id: folderButton
                unit: panel.unit
                text: qsTr("Andere map…")
                enabled: !recorder.recording
                onClicked: folderDialog.open()
                KeyNavigation.right: openButton
                KeyNavigation.down: durationRow
            }
            FocusButton {
                id: openButton
                unit: panel.unit
                text: qsTr("Map openen")
                onClicked: recorder.openFolder()
                KeyNavigation.left: folderButton
                KeyNavigation.down: durationRow
            }
        }

        // Duur
        Text { color: "#9fb4d0"; font.pixelSize: (panel.unit * 2.2) * appModel.fontScale; text: qsTr("Duur")}
        FocusScope {
            id: durationRow
            width: content.width
            height: panel.unit * 6
            property int current: panel.durationIndex
            onActiveFocusChanged: if (activeFocus) durationButtons.itemAt(panel.durationIndex).forceActiveFocus()
            KeyNavigation.up: folderButton
            KeyNavigation.down: recorder.recording ? stopButton : startButton
            Row {
                spacing: panel.unit * 1.2
                Repeater {
                    id: durationButtons
                    model: panel.durations
                    delegate: FocusButton {
                        required property int modelData
                        required property int index
                        unit: panel.unit
                        text: modelData === 0 ? qsTr("Tot ik stop") : (modelData >= 60 ? (modelData / 60) + qsTr(" uur") : modelData + qsTr(" min"))
                        checked: index === panel.durationIndex
                        enabled: !recorder.recording
                        onClicked: panel.durationIndex = index
                        KeyNavigation.left: index > 0 ? durationButtons.itemAt(index - 1) : null
                        KeyNavigation.right: index < panel.durations.length - 1 ? durationButtons.itemAt(index + 1) : null
                        KeyNavigation.up: folderButton
                        KeyNavigation.down: recorder.recording ? stopButton : startButton
                    }
                }
            }
        }

        // Start / stop + voortgang
        Row {
            spacing: panel.unit * 2
            FocusButton {
                id: startButton
                unit: panel.unit
                text: qsTr("●  Opname starten")
                visible: !recorder.recording
                onClicked: panel.startRecording()
                KeyNavigation.up: durationRow
                KeyNavigation.down: planned.visible ? planned : list
            }
            FocusButton {
                id: stopButton
                unit: panel.unit
                text: qsTr("■  Opname stoppen")
                visible: recorder.recording
                onClicked: recorder.stop()
                KeyNavigation.up: durationRow
                KeyNavigation.down: planned.visible ? planned : list
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: panel.unit * 0.4
                Row {
                    spacing: panel.unit
                    Rectangle {
                        visible: recorder.recording
                        anchors.verticalCenter: parent.verticalCenter
                        width: panel.unit * 1.8; height: width; radius: width / 2
                        color: "#e53935"
                        SequentialAnimation on opacity {
                            running: recorder.recording; loops: Animation.Infinite
                            NumberAnimation { to: 0.2; duration: 600 } NumberAnimation { to: 1; duration: 600 }
                        }
                    }
                    Text {
                        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                        color: "white"
                        font.pixelSize: (panel.unit * 2.6) * appModel.fontScale
                        text: recorder.recording
                              ? recorder.channel + "   " + panel.fmtTime(recorder.elapsedSeconds)
                                + (recorder.remainingSeconds >= 0 ? qsTr("   (nog ") + panel.fmtTime(recorder.remainingSeconds) + ")" : "")
                                + "   " + panel.fmtSize(recorder.bytes)
                              : (recorder.status !== "" ? recorder.status : (recorder.available ? "" : qsTr("ffmpeg ontbreekt: opnemen kan niet")))
                    }
                }
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    visible: recorder.recording
                    color: "#b0b4bb"
                    font.pixelSize: (panel.unit * 2) * appModel.fontScale
                    elide: Text.ElideMiddle
                    width: content.width - stopButton.width - panel.unit * 4
                    text: recorder.status + "   ·   " + recorder.file
                }
            }
        }
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            id: message
            color: "#ffb74d"
            font.pixelSize: (panel.unit * 2.2) * appModel.fontScale
            visible: text !== ""
        }

        // Geplande opnames (uit de tv-gids)
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            visible: recorder.schedule.length > 0
            color: "#9fb4d0"
            font.pixelSize: (panel.unit * 2.2) * appModel.fontScale
            text: qsTr("Gepland") + (gamepad.connectedCount > 0 ? qsTr("   ·   Ⓧ annuleren") : qsTr("   ·   F2 of rechtermuisknop: annuleren"))
                  + qsTr("   ·   Laat Kijkluister aan staan; de computer gaat tijdens een opname niet slapen")
        }
        ListView {
            id: planned
            visible: recorder.schedule.length > 0
            width: content.width
            height: Math.min(count, 4) * panel.unit * 5
            clip: true
            interactive: count > 4
            model: recorder.schedule
            KeyNavigation.up: recorder.recording ? stopButton : startButton
            KeyNavigation.down: list
            Keys.onPressed: (event) => {
                if ((event.key === Qt.Key_F2 || event.key === Qt.Key_Menu) && currentItem) {
                    recorder.removeFromSchedule(currentItem.modelData.id)
                    event.accepted = true
                }
            }
            function when(m) {
                const d = new Date(m.start * 1000), e = new Date(m.stop * 1000)
                const pad = n => (n < 10 ? "0" : "") + n
                return d.toLocaleDateString(Qt.locale(), "ddd d MMM") + "  " + pad(d.getHours()) + ":" + pad(d.getMinutes())
                       + " – " + pad(e.getHours()) + ":" + pad(e.getMinutes())
            }
            delegate: Rectangle {
                id: prow
                required property var modelData
                required property int index
                readonly property bool current: ListView.isCurrentItem && planned.activeFocus
                width: planned.width
                height: panel.unit * 4.6
                radius: panel.unit
                color: current ? "#ffffff" : "transparent"
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        planned.currentIndex = prow.index
                        planned.forceActiveFocus()
                        if (mouse.button === Qt.RightButton) recorder.removeFromSchedule(prow.modelData.id)
                    }
                }
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    x: panel.unit * 1.5
                    spacing: panel.unit * 2
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: panel.unit * 1.4; height: width; radius: width / 2
                        color: prow.modelData.state === "bezig" ? "#e53935" : prow.modelData.state === "gepland" ? "#ff8a80" : "#9e9e9e"
                    }
                    Text {
                        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                        width: planned.width * 0.28
                        color: prow.current ? "#202124" : "#d5d8dc"
                        font.pixelSize: (panel.unit * 2.2) * appModel.fontScale
                        text: planned.when(prow.modelData)
                    }
                    Text {
                        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                        width: planned.width * 0.45
                        elide: Text.ElideRight
                        color: prow.current ? "#202124" : "white"
                        font.pixelSize: (panel.unit * 2.3) * appModel.fontScale
                        text: prow.modelData.title + "  ·  " + prow.modelData.channel
                    }
                    Text {
                        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                        color: prow.current ? "#5f6368" : "#b0b4bb"
                        font.pixelSize: (panel.unit * 2.1) * appModel.fontScale
                        text: prow.modelData.state === "bezig" ? qsTr("wordt opgenomen")
                              : prow.modelData.state === "overlap" ? qsTr("overgeslagen: er liep al een opname")
                              : prow.modelData.state === "mislukt" ? "mislukt" : "gepland"
                    }
                }
            }
        }

        // Eerdere opnames
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            color: "#9fb4d0"
            font.pixelSize: (panel.unit * 2.2) * appModel.fontScale
            text: qsTr("Opnames in deze map") + (recorder.recordings.length > 0
                  ? (gamepad.connectedCount > 0 ? qsTr("   ·   Ⓐ afspelen   ·   Ⓧ verwijderen") : qsTr("   ·   Enter: afspelen   ·   F2: verwijderen")) : "")
        }
        ListView {
            id: list
            width: content.width
            height: panel.height - y - panel.unit * 10
            clip: true
            model: recorder.recordings
            spacing: panel.unit * 0.4
            KeyNavigation.up: planned.visible ? planned : (recorder.recording ? stopButton : startButton)
            Keys.onReturnPressed: if (currentItem) recorder.play(currentItem.modelData.path)
            Keys.onEnterPressed: if (currentItem) recorder.play(currentItem.modelData.path)
            Keys.onPressed: (event) => {
                if ((event.key === Qt.Key_F2 || event.key === Qt.Key_Menu) && currentItem) {
                    if (panel.confirmDelete === currentIndex) {
                        recorder.remove(currentItem.modelData.path)
                        panel.confirmDelete = -1
                    } else {
                        panel.confirmDelete = currentIndex
                    }
                    event.accepted = true
                }
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                visible: list.count === 0
                color: "#a8acb3"
                font.pixelSize: (panel.unit * 2.4) * appModel.fontScale
                text: qsTr("Nog geen opnames")
            }
            delegate: Rectangle {
                id: row
                required property var modelData
                required property int index
                readonly property bool current: ListView.isCurrentItem && list.activeFocus
                width: list.width
                height: panel.unit * 6.5
                radius: panel.unit
                color: current ? "#ffffff" : (index % 2 ? "#14ffffff" : "transparent")
                MouseArea {
                    anchors.fill: parent
                    onClicked: { list.currentIndex = row.index; list.forceActiveFocus() }
                    onDoubleClicked: recorder.play(row.modelData.path)
                }
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    x: panel.unit * 1.5
                    spacing: panel.unit * 3
                    Text {
                        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                        width: list.width * 0.55
                        elide: Text.ElideRight
                        color: row.current ? "#202124" : "white"
                        font.pixelSize: (panel.unit * 2.5) * appModel.fontScale
                        text: (row.modelData.busy ? "●  " : "") + row.modelData.name
                    }
                    Text {
                        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                        width: list.width * 0.18
                        color: row.current ? "#5f6368" : "#b0b4bb"
                        font.pixelSize: (panel.unit * 2.2) * appModel.fontScale
                        text: row.modelData.date
                    }
                    Text {
                        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                        color: row.current ? "#5f6368" : "#b0b4bb"
                        font.pixelSize: (panel.unit * 2.2) * appModel.fontScale
                        text: panel.confirmDelete === row.index ? (gamepad.connectedCount > 0 ? qsTr("Nog eens Ⓧ om te verwijderen") : qsTr("Nog eens F2 om te verwijderen")) : panel.fmtSize(row.modelData.size)
                    }
                }
            }
        }
    }
}
