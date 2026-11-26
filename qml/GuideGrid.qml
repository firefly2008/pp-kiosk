// Gidsraster (4.4): alle zenders van de huidige lijst onder elkaar, programma's op een tijdlijn van 3 uur.
// ↑↓ zender, ←→ programma (de tijdlijn schuift mee), Enter = kijken (nu) of herinnering (later),
// R = opnemen, H = herinnering, Esc = terug.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

FocusScope {
    id: gg

    readonly property real unit: height / 100
    property Item returnFocusTo: null
    readonly property int rowsShown: 8
    readonly property real span: 3 * 3600          // seconden zichtbaar
    property real windowStart: 0                   // begin van de tijdlijn (halve uren)
    property int firstRow: 0
    property int selRow: 0
    property real selTime: 0                       // tijdstip "in" het gekozen programma
    property var rowsData: []
    property int revision: 0
    signal watchRequested(int row)

    anchors.fill: parent
    visible: false
    z: 45

    function halfHour(secs) { return Math.floor(secs / 1800) * 1800 }
    function nowSecs() { return Date.now() / 1000 }

    function open(row, focusItem) {
        returnFocusTo = focusItem
        windowStart = halfHour(nowSecs())
        selTime = nowSecs()
        selRow = Math.max(0, Math.min(row, iptv.count - 1))
        firstRow = Math.max(0, selRow - 2)
        refresh()
        visible = true
        forceActiveFocus()
        console.info("Guide grid opened", iptv.count, "channels")
    }
    function close() {
        visible = false
        if (returnFocusTo) returnFocusTo.forceActiveFocus()
    }
    function refresh() {
        rowsData = iptv.guideGrid(firstRow, rowsShown, windowStart, windowStart + span)
        // Xtream: gids van de zichtbare zenders ophalen
        for (let r = firstRow; r < firstRow + rowsShown && r < iptv.count; ++r) iptv.requestEpg(r)
    }
    Connections {
        target: iptv
        function onEpgChanged() { if (gg.visible) gg.refresh() }
    }

    // Gekozen programma (of null als de zender op dat moment niets heeft)
    function programmeAt(rowIndex, t) {
        const data = rowsData[rowIndex - firstRow]
        if (!data) return null
        const list = data.programmes
        for (let i = 0; i < list.length; ++i) {
            if (list[i].startSecs <= t && list[i].stopSecs > t) return list[i]
        }
        return null
    }
    readonly property var selected: (revision, rowsData, programmeAt(selRow, Math.max(selTime, windowStart)))
    readonly property var selectedChannel: rowsData[selRow - firstRow] || null

    function moveRow(delta) {
        const next = Math.max(0, Math.min(iptv.count - 1, selRow + delta))
        if (next === selRow) return
        selRow = next
        if (selRow < firstRow) firstRow = selRow
        else if (selRow >= firstRow + rowsShown) firstRow = selRow - rowsShown + 1
        else { revision++; return }
        refresh()
    }
    function moveTime(delta) {
        const p = selected
        const t = Math.max(selTime, windowStart)
        if (delta > 0) selTime = p ? p.stopSecs : t + 1800
        else selTime = p ? p.startSecs - 1 : t - 1800
        // Niet verder terug dan een uur geleden
        selTime = Math.max(selTime, halfHour(nowSecs()) - 3600)
        let changed = false
        while (selTime >= windowStart + span - 1800) { windowStart += 1800; changed = true }
        while (selTime < windowStart) { windowStart -= 1800; changed = true }
        if (changed) refresh()
        else revision++
    }
    function activate() {
        const p = selected
        if (!selectedChannel) return
        if (!p || p.isNow || p.startSecs <= nowSecs()) {
            close()
            gg.watchRequested(selRow)
        } else {
            remind()
        }
    }
    function remind() {
        const p = selected, ch = selectedChannel
        if (!p || !ch || p.startSecs <= nowSecs()) return
        iptv.toggleReminder(ch.name, ch.url, ch.logo, p.title, p.startSecs)
        revision++
        toastText.text = iptv.isReminder(ch.url, p.startSecs) ? qsTr("🔔 Herinnering: ") + p.title + qsTr(" om ") + p.start
                                                               : qsTr("Herinnering weg: ") + p.title
        toastTimer.restart()
    }
    function record() {
        const p = selected, ch = selectedChannel
        if (!p || !ch) return
        recorder.toggleSchedule(ch.name, ch.url, p.title, p.startSecs, p.stopSecs)
        revision++
        toastText.text = recorder.isScheduled(ch.url, p.startSecs) ? qsTr("● Wordt opgenomen: ") + p.title : qsTr("Opname geannuleerd: ") + p.title
        toastTimer.restart()
    }

    Keys.onUpPressed: moveRow(-1)
    Keys.onDownPressed: moveRow(1)
    Keys.onLeftPressed: moveTime(-1)
    Keys.onRightPressed: moveTime(1)
    Keys.onReturnPressed: activate()
    Keys.onEnterPressed: activate()
    Keys.onEscapePressed: close()
    Keys.onBackPressed: close()
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_R || event.key === Qt.Key_MediaRecord) { record(); event.accepted = true }
        else if (event.key === Qt.Key_H) { remind(); event.accepted = true }
        else if (event.key === Qt.Key_G) { close(); event.accepted = true }
        else if (event.key === Qt.Key_PageDown) { moveRow(rowsShown); event.accepted = true }
        else if (event.key === Qt.Key_PageUp) { moveRow(-rowsShown); event.accepted = true }
    }

    Rectangle { anchors.fill: parent; color: "#0b0c0f"; MouseArea { anchors.fill: parent } }

    // Kop met het gekozen programma
    Column {
        id: head
        x: gg.unit * 5
        y: gg.unit * 3.5
        width: parent.width - x * 2
        spacing: gg.unit * 0.8
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            text: qsTr("Gids  ·  ") + qsTr(iptv.group || "Alle zenders")
            color: "#a8acb3"
            font.pixelSize: gg.unit * 2.2 * appModel.fontScale
        }
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            width: parent.width
            elide: Text.ElideRight
            color: "white"
            font.pixelSize: gg.unit * 4 * appModel.fontScale
            font.weight: Font.DemiBold
            text: gg.selected ? gg.selected.title : (gg.selectedChannel ? gg.selectedChannel.name : "")
        }
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            width: parent.width
            elide: Text.ElideRight
            color: "#ffb74d"
            font.pixelSize: gg.unit * 2.2 * appModel.fontScale
            text: gg.selected ? gg.selected.start + " – " + gg.selected.stop + "  ·  " + (gg.selectedChannel ? gg.selectedChannel.name : "")
                                + (gg.revision >= 0 && gg.selectedChannel && recorder.isScheduled(gg.selectedChannel.url, gg.selected.startSecs) ? qsTr("  ·  ● opname gepland") : "")
                                + (gg.revision >= 0 && gg.selectedChannel && iptv.isReminder(gg.selectedChannel.url, gg.selected.startSecs) ? qsTr("  ·  🔔 herinnering") : "")
                              : qsTr("Geen gidsinformatie")
        }
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            width: parent.width
            height: gg.unit * 6
            wrapMode: Text.Wrap
            elide: Text.ElideRight
            maximumLineCount: 2
            color: "#c9ccd1"
            font.pixelSize: gg.unit * 2 * appModel.fontScale
            text: gg.selected ? (gg.selected.description || "") : ""
        }
    }

    // Tijdlijn
    readonly property real channelWidth: width * 0.2
    readonly property real timelineX: unit * 5 + channelWidth + unit
    readonly property real timelineWidth: width - timelineX - unit * 5
    function xFor(secs) { return (Math.max(secs, windowStart) - windowStart) / span * timelineWidth }

    Item {
        id: times
        x: gg.timelineX
        y: head.y + head.height + gg.unit * 2
        width: gg.timelineWidth
        height: gg.unit * 4
        Repeater {
            model: 6
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                required property int index
                x: index * gg.timelineWidth / 6
                color: "#a8acb3"
                font.pixelSize: gg.unit * 2 * appModel.fontScale
                text: { const d = new Date((gg.windowStart + index * 1800) * 1000); return Qt.formatTime(d, qsTr("HH:mm")) }
            }
        }
    }

    Column {
        id: rows
        x: gg.unit * 5
        y: times.y + times.height
        width: parent.width - x * 2
        spacing: gg.unit * 0.6
        Repeater {
            model: gg.rowsData
            Item {
                id: rowItem
                required property var modelData
                required property int index
                readonly property bool current: modelData.row === gg.selRow
                width: rows.width
                height: (gg.height - rows.y - gg.unit * 8) / gg.rowsShown - rows.spacing
                // Zender
                Rectangle {
                    width: gg.channelWidth
                    height: parent.height
                    radius: gg.unit * 0.8
                    color: rowItem.current ? "#2b3340" : "#191c22"
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        x: gg.unit
                        spacing: gg.unit
                        Text {
                            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#80848b"
                            font.pixelSize: gg.unit * 1.8 * appModel.fontScale
                            text: rowItem.modelData.row + 1
                        }
                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            width: gg.unit * 5; height: gg.unit * 3.5
                            source: iptv.logoSource(rowItem.modelData.logo || "")
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            onStatusChanged: if (status === Image.Error) iptv.markLogoFailed(rowItem.modelData.logo || "")
                        }
                        Text {
                            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                            anchors.verticalCenter: parent.verticalCenter
                            width: gg.channelWidth - gg.unit * 11
                            elide: Text.ElideRight
                            color: "white"
                            font.pixelSize: gg.unit * 2 * appModel.fontScale
                            text: rowItem.modelData.name
                        }
                    }
                }
                // Programma's
                Item {
                    x: gg.timelineX - rows.x
                    width: gg.timelineWidth
                    height: parent.height
                    clip: true
                    Rectangle {
                        anchors.fill: parent
                        visible: rowItem.modelData.programmes.length === 0
                        radius: gg.unit * 0.8
                        color: rowItem.current ? "#e8eaed" : "#15181d"
                        Text {
                            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                            anchors.verticalCenter: parent.verticalCenter
                            x: gg.unit * 1.2
                            color: rowItem.current ? "#111318" : "#80848b"
                            font.pixelSize: gg.unit * 2 * appModel.fontScale
                            text: qsTr("Geen gidsinformatie")
                        }
                    }
                    Repeater {
                        model: rowItem.modelData.programmes
                        Rectangle {
                            id: prog
                            required property var modelData
                            readonly property bool chosen: rowItem.current && gg.selected !== null
                                                           && gg.selected.startSecs === modelData.startSecs
                            x: gg.xFor(modelData.startSecs) + 1
                            width: Math.max(gg.unit, gg.xFor(Math.min(modelData.stopSecs, gg.windowStart + gg.span)) - x - 2)
                            height: parent.height
                            radius: gg.unit * 0.8
                            color: chosen ? "#e8eaed" : modelData.isNow ? "#26303d" : "#1d2026"
                            Text {
                                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                                anchors.verticalCenter: parent.verticalCenter
                                x: gg.unit
                                width: parent.width - gg.unit * 2
                                elide: Text.ElideRight
                                color: prog.chosen ? "#111318" : "white"
                                font.pixelSize: gg.unit * 2 * appModel.fontScale
                                text: (gg.revision >= 0 && recorder.isScheduled(rowItem.modelData.url, prog.modelData.startSecs) ? "● " : "")
                                      + (gg.revision >= 0 && iptv.isReminder(rowItem.modelData.url, prog.modelData.startSecs) ? "🔔 " : "")
                                      + prog.modelData.title
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: { gg.selRow = rowItem.modelData.row; gg.selTime = prog.modelData.startSecs; gg.revision++ }
                                onDoubleClicked: gg.activate()
                            }
                        }
                    }
                }
            }
        }
    }

    // Lijn "nu"
    Rectangle {
        readonly property real now: (clock.tick, gg.nowSecs())
        visible: now >= gg.windowStart && now < gg.windowStart + gg.span
        x: gg.timelineX + gg.xFor(now)
        y: times.y
        width: 2
        height: rows.y + rows.height - times.y
        color: "#e5484d"
    }
    Timer { id: clock; property int tick: 0; interval: 30000; repeat: true; running: gg.visible; onTriggered: tick++ }

    HintBar {
        anchors.left: parent.left
        anchors.leftMargin: gg.unit * 5
        anchors.bottom: parent.bottom
        anchors.bottomMargin: gg.unit * 2.5
        unit: gg.unit
        items: [{ pad: "✛", key: "↑ ↓ ← →", label: qsTr("Kiezen") }, { pad: "A", key: qsTr("Enter"), label: qsTr("Kijken / herinneren") },
                { pad: qsTr("RB"), key: "R", label: qsTr("Opnemen") }, { pad: "", key: "H", label: qsTr("Herinnering") }, { pad: "B", key: qsTr("Esc"), label: qsTr("Terug") }]
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: gg.unit * 8
        width: toastText.implicitWidth + gg.unit * 6
        height: gg.unit * 6.5
        radius: height / 2
        color: "#f0202124"
        opacity: toastTimer.running ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        Text { id: toastText; textFormat: Text.PlainText; anchors.centerIn: parent; color: "white"; font.pixelSize: gg.unit * 2.4 * appModel.fontScale }
    }
    Timer { id: toastTimer; interval: 4000 }
}
