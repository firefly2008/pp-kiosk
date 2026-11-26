// Tv-gids van één zender: lijst met programma's links, uitgebreide info rechts.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

FocusScope {
    id: panel

    property real unit: height / 100
    property int row: -1
    property var channel: ({})
    property Item returnFocusTo: null
    property var programmes: []
    property int startIndex: 0       // handleiding: ander programma voorselecteren

    anchors.fill: parent
    visible: false
    z: 40

    function open(r, ch, focusItem) {
        row = r
        channel = ch
        returnFocusTo = focusItem
        refresh()
        list.currentIndex = Math.min(startIndex, Math.max(0, programmes.length - 1))
        visible = true
        list.forceActiveFocus()
    }

    function refresh() {
        programmes = row >= 0 ? iptv.guide(row) : []
    }

    function close() {
        visible = false
        if (returnFocusTo) {
            returnFocusTo.forceActiveFocus()
        }
    }

    Connections {
        target: iptv
        function onEpgChanged() {
            if (panel.visible) {
                const keep = list.currentIndex
                panel.refresh()
                list.currentIndex = Math.min(Math.max(0, keep), panel.programmes.length - 1)
            }
        }
    }

    Keys.onEscapePressed: close()
    Keys.onBackPressed: close()
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_I) {
            close()
            event.accepted = true
        } else if (event.key === Qt.Key_R) {
            toggleRecord(list.currentIndex)
            event.accepted = true
        } else if (event.key === Qt.Key_H) {
            toggleReminder(list.currentIndex)
            event.accepted = true
        } else if (event.key === Qt.Key_S) {
            toggleSeries(list.currentIndex)
            event.accepted = true
        }
    }

    // Herinnering (2 minuten voor het begin een melding) en serie opnemen (elke uitzending met deze titel)
    property int extraRevision: 0
    Connections {
        target: iptv
        function onRemindersChanged() { panel.extraRevision++ }
        function onSeriesChanged() { panel.extraRevision++ }
    }
    function hasReminder(p) {
        return extraRevision >= 0 && p && channel.url ? iptv.isReminder(channel.url, p.startSecs) : false
    }
    function isSeries(p) {
        return extraRevision >= 0 && p && channel.url ? iptv.isSeries(channel.url, p.title) : false
    }
    function toggleReminder(index) {
        const p = index >= 0 && index < programmes.length ? programmes[index] : null
        if (!p || !channel.url || p.isNow) {
            return
        }
        iptv.toggleReminder(channel.name, channel.url, channel.logo || "", p.title, p.startSecs)
        const on = iptv.isReminder(channel.url, p.startSecs)
        recordToastTitle.text = on ? qsTr("🔔 Herinnering: ") + p.title : qsTr("Herinnering weg: ") + p.title
        recordToastText.text = on ? p.day.toLowerCase() + " " + p.start + " · " + channel.name
                                    + qsTr("\nTwee minuten van tevoren krijg je een melding, ook als je iets anders kijkt.") : ""
        recordToastTimer.restart()
    }
    function toggleSeries(index) {
        const p = index >= 0 && index < programmes.length ? programmes[index] : null
        if (!p || !channel.url) {
            return
        }
        iptv.toggleSeries(channel.name, channel.url, p.title)
        const on = iptv.isSeries(channel.url, p.title)
        recordToastTitle.text = on ? qsTr("● Serie wordt opgenomen: ") + p.title : qsTr("Serie-opname gestopt: ") + p.title
        recordToastText.text = on ? qsTr("Elke uitzending van \u201c") + p.title + qsTr("\u201d op ") + channel.name + qsTr(" wordt opgenomen.\nLaat Kijkluister aan staan.") : ""
        recordToastTimer.restart()
    }

    // Programma opnemen (of de geplande opname weer weghalen)
    function isPlanned(p) {
        return recorderRevision >= 0 && p && channel.url ? recorder.isScheduled(channel.url, p.startSecs) : false
    }
    property int recorderRevision: 0
    Connections {
        target: recorder
        function onScheduleChanged() { panel.recorderRevision++ }
    }
    function toggleRecord(index) {
        const p = index >= 0 && index < programmes.length ? programmes[index] : null
        if (!p || !channel.url) {
            return
        }
        const was = isPlanned(p)
        recorder.toggleSchedule(channel.name, channel.url, p.title, p.startSecs, p.stopSecs)
        recordToastTitle.text = was ? qsTr("Opname geannuleerd: ") + p.title : qsTr("Wordt opgenomen: ") + p.title
        recordToastText.text = was ? "" : p.day.toLowerCase() + " " + p.start + " – " + p.stop + " · " + channel.name
                                         + qsTr("\nLaat Kijkluister aan staan. Je vindt de opname straks bij Live tv → Opnemen.")
        recordToastTimer.restart()
    }

    Rectangle {
        anchors.fill: parent
        color: "#0b0c0f"
        MouseArea { anchors.fill: parent; onClicked: {} }
    }

    // Kop: logo, zender, sluiten-hint
    Row {
        id: header
        x: panel.unit * 6
        y: panel.unit * 5
        spacing: panel.unit * 2.5
        Rectangle {
            width: panel.unit * 10
            height: panel.unit * 7
            radius: panel.unit
            color: "#22252b"
            Image {
                anchors.fill: parent
                anchors.margins: panel.unit * 0.8
                source: iptv.logoSource(panel.channel.logo || "")
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                onStatusChanged: if (status === Image.Error) iptv.markLogoFailed(panel.channel.logo || "")
            }
        }
        Column {
            anchors.verticalCenter: parent.verticalCenter
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                color: "white"
                font.pixelSize: (panel.unit * 4) * appModel.fontScale
                font.weight: Font.DemiBold
                text: qsTr("Tv-gids  ·  ") + (panel.channel.name || "")
            }
            HintBar {
                unit: panel.unit * 0.9
                items: [
                    { pad: "✛", key: "↑ ↓", label: qsTr("Programma kiezen") },
                    { pad: qsTr("RB"), key: "R", label: qsTr("Opnemen") },
                    { pad: "", key: "H", label: qsTr("Herinnering") },
                    { pad: "", key: "S", label: qsTr("Serie") },
                    { pad: "B", key: qsTr("Esc"), label: qsTr("Sluiten") }
                ]
            }
        }
    }

    Text {
        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
        anchors.centerIn: parent
        visible: panel.programmes.length === 0
        color: "#b0b4bb"
        font.pixelSize: (panel.unit * 3) * appModel.fontScale
        text: iptv.epgStatus === qsTr("Gids laden…") ? qsTr("Gids laden…") : qsTr("Geen gidsinformatie voor deze zender")
    }

    // Programmalijst
    ListView {
        id: list
        x: panel.unit * 6
        y: panel.unit * 16
        width: parent.width * 0.38
        height: parent.height - y - panel.unit * 5
        clip: true
        model: panel.programmes
        spacing: panel.unit * 0.6
        highlightMoveDuration: 120
        keyNavigationWraps: false
        Keys.onReturnPressed: {}
        delegate: Rectangle {
            id: item
            required property var modelData
            required property int index
            readonly property bool current: ListView.isCurrentItem
            readonly property bool newDay: index === 0 || panel.programmes[index - 1].day !== modelData.day
            width: list.width
            height: panel.unit * (newDay ? 10.5 : 7)
            radius: panel.unit
            color: current ? "#ffffff" : "transparent"

            MouseArea {
                anchors.fill: parent
                onClicked: { list.currentIndex = item.index; list.forceActiveFocus() }
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                visible: item.newDay
                x: panel.unit * 1.5
                y: panel.unit * 0.6
                color: item.current ? "#c0392b" : "#ffb74d"
                font.pixelSize: (panel.unit * 2) * appModel.fontScale
                font.weight: Font.DemiBold
                text: item.modelData.day
            }
            Row {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: panel.unit * 1.6
                x: panel.unit * 1.5
                spacing: panel.unit * 2
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    color: item.current ? "#202124" : "#b0b4bb"
                    font.pixelSize: (panel.unit * 2.6) * appModel.fontScale
                    text: item.modelData.start
                }
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    width: list.width - panel.unit * 17
                    elide: Text.ElideRight
                    color: item.current ? "#202124" : "white"
                    font.pixelSize: (panel.unit * 2.6) * appModel.fontScale
                    font.weight: item.modelData.isNow ? Font.DemiBold : Font.Normal
                    text: (item.modelData.isNow ? "▶ " : "") + item.modelData.title
                }
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    visible: panel.hasReminder(item.modelData)
                    anchors.verticalCenter: parent.verticalCenter
                    font.pixelSize: (panel.unit * 2) * appModel.fontScale
                    text: "🔔"
                }
                Rectangle {
                    visible: panel.isPlanned(item.modelData)
                    anchors.verticalCenter: parent.verticalCenter
                    width: panel.unit * 1.6; height: width; radius: width / 2
                    color: "#e53935"
                }
            }
        }
    }

    // Uitgebreide info van het gekozen programma
    Flickable {
        id: details
        readonly property var p: list.currentIndex >= 0 && list.currentIndex < panel.programmes.length
                                 ? panel.programmes[list.currentIndex] : null
        x: list.x + list.width + panel.unit * 5
        y: list.y
        width: parent.width - x - panel.unit * 6
        height: list.height
        clip: true
        contentHeight: info.implicitHeight
        visible: p !== null

        Column {
            id: info
            width: details.width
            spacing: panel.unit * 1.5
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                wrapMode: Text.Wrap
                color: "white"
                font.pixelSize: (panel.unit * 4.5) * appModel.fontScale
                font.weight: Font.DemiBold
                text: details.p ? details.p.title : ""
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                color: "#ffb74d"
                font.pixelSize: (panel.unit * 2.4) * appModel.fontScale
                text: details.p ? details.p.day + "   " + details.p.start + " – " + details.p.stop
                                  + "   ·   " + details.p.duration + qsTr(" min")
                                  + (details.p.category ? "   ·   " + details.p.category : "") : ""
            }
            Rectangle {
                visible: details.p && details.p.isNow
                width: panel.unit * 40
                height: panel.unit * 0.6
                radius: height / 2
                color: "#40ffffff"
                Rectangle {
                    width: parent.width * (details.p ? details.p.progress : 0)
                    height: parent.height
                    radius: height / 2
                    color: "#ffb74d"
                }
            }
            Row {
                spacing: panel.unit * 1.5
                FocusButton {
                    id: recordButton
                    unit: panel.unit
                    text: details.p && panel.isPlanned(details.p)
                          ? (details.p.isNow ? qsTr("■  Opname stoppen") : qsTr("Opname annuleren"))
                          : qsTr("●  Dit programma opnemen")
                    onClicked: panel.toggleRecord(list.currentIndex)
                    KeyNavigation.right: remindButton
                }
                FocusButton {
                    id: remindButton
                    visible: details.p && !details.p.isNow
                    unit: panel.unit
                    text: details.p && panel.hasReminder(details.p) ? qsTr("Herinnering weg") : qsTr("🔔  Herinner me")
                    onClicked: panel.toggleReminder(list.currentIndex)
                    KeyNavigation.left: recordButton
                    KeyNavigation.right: seriesButton
                }
                FocusButton {
                    id: seriesButton
                    unit: panel.unit
                    text: details.p && panel.isSeries(details.p) ? qsTr("Serie-opname stoppen") : qsTr("Serie opnemen")
                    onClicked: panel.toggleSeries(list.currentIndex)
                    KeyNavigation.left: remindButton.visible ? remindButton : recordButton
                }

            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                wrapMode: Text.Wrap
                lineHeight: 1.25
                color: "#d5d8dc"
                font.pixelSize: (panel.unit * 2.7) * appModel.fontScale
                text: details.p ? (details.p.description || qsTr("Geen beschrijving beschikbaar.")) : ""
            }
        }
    }

    // Duidelijke bevestiging onderin na opnemen of annuleren
    Rectangle {
        z: 20
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: panel.unit * 4
        width: Math.min(parent.width * 0.8, Math.max(recordToastTitle.implicitWidth, recordToastText.implicitWidth) + panel.unit * 8)
        height: recordToastColumn.implicitHeight + panel.unit * 4
        radius: panel.unit * 1.5
        color: "#f0202124"
        border.color: "#e53935"
        border.width: 2
        opacity: recordToastTimer.running ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        Row {
            anchors.centerIn: parent
            spacing: panel.unit * 2
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: panel.unit * 2.2; height: width; radius: width / 2
                color: "#e53935"
            }
            Column {
                id: recordToastColumn
                spacing: panel.unit * 0.6
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    id: recordToastTitle
                    color: "white"
                    font.pixelSize: (panel.unit * 3) * appModel.fontScale
                    font.weight: Font.DemiBold
                }
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    id: recordToastText
                    visible: text !== ""
                    color: "#d5d8dc"
                    font.pixelSize: (panel.unit * 2.3) * appModel.fontScale
                    lineHeight: 1.2
                }
            }
        }
    }
    Timer { id: recordToastTimer; interval: 7000 }
}
