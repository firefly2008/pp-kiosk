// Welkomststap: "Welke diensten gebruik je?" Aangevinkte diensten blijven op het startscherm,
// de rest wordt verborgen (via Instellingen > Alle verborgen tegels terugzetten weer terug te halen).
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

FocusScope {
    id: welcome

    readonly property real unit: height / 100
    property var choices: []           // [{ model, name, icon, accent, on }]
    property Item returnFocusTo: null
    signal done()

    // Populaire diensten in Nederland: voorgeselecteerd bij een nieuwe installatie
    readonly property var popular: ["Netflix", "NPO Start", "YouTube", "Videoland", "Disney+", "Prime Video",
                                    "Spotify", "YouTube Music", "NPO Luister", "NOS", "NU.nl"]

    anchors.fill: parent
    visible: false
    z: 300

    function open(focusItem) {
        returnFocusTo = focusItem
        const list = []
        // Kopje over de hele breedte; daarna de tegels; rij opvullen met lege vakjes
        const header = (title) => {
            while (list.length % 5 !== 0) list.push({ spacer: true })
            list.push({ header: title })
            for (let k = 1; k < 5; ++k) list.push({ spacer: true })
        }
        const kinds = new Map([[webVideo, qsTr("Streaming")], [webMusic, qsTr("Muziek")], [webPodcasts, qsTr("Podcasts")], [webNews, qsTr("Nieuws")]])
        for (const m of [webVideo, webMusic, webPodcasts, webNews]) {
            header(kinds.get(m))
            // Wat nu zichtbaar is, blijft aangevinkt; verborgen diensten komen er even bij
            const visibleNow = []
            for (let i = 0; i < m.count; ++i) visibleNow.push(m.nameAt(i))
            m.restoreHidden()
            for (let i = 0; i < m.count; ++i) {
                const name = m.nameAt(i)
                const on = appModel.freshInstall ? popular.indexOf(name) >= 0 : visibleNow.indexOf(name) >= 0
                list.push({ model: m, name: name, icon: m.iconAt(i), accent: m.accentAt(i), on: on })
            }
        }
        // Alle Nederlandse radiozenders
        const topRadio = ["NPO Radio 2", "Qmusic", "Radio 538", "Sky Radio", "Radio 10", "JOE", "Radio Veronica"]
        header(qsTr("Radio"))
        for (const r of radio.catalog()) {
            list.push({ model: radio, name: r.name, icon: r.icon, accent: r.accent,
                        on: appModel.freshInstall ? topRadio.indexOf(r.name) >= 0 : r.on })
        }
        choices = list
        grid.currentIndex = 5
        visible = true
        grid.forceActiveFocus()
    }

    function selectable(i) {
        return i >= 0 && i < choices.length && !choices[i].header && !choices[i].spacer
    }

    // Pijltjes: kopjes en lege vakjes overslaan
    function step(dx, dy) {
        let i = grid.currentIndex
        if (dx !== 0) {
            do { i += dx } while (i >= 0 && i < choices.length && !selectable(i))
            if (selectable(i)) grid.currentIndex = i
            return true
        }
        const col = grid.currentIndex % 5
        let row = Math.floor(grid.currentIndex / 5) + dy
        const rows = Math.ceil(choices.length / 5)
        while (row >= 0 && row < rows) {
            // Zelfde kolom, anders het dichtstbijzijnde vakje in die rij
            for (let c = col; c >= 0; --c) {
                if (selectable(row * 5 + c)) { grid.currentIndex = row * 5 + c; return true }
            }
            row += dy
        }
        return false
    }

    function toggle(i) {
        if (!selectable(i)) return
        const c = choices.slice()
        c[i] = Object.assign({}, c[i], { on: !c[i].on })
        choices = c
    }

    function finish() {
        // Van achter naar voren verbergen, zodat de rijnummers kloppen
        for (const m of [webVideo, webMusic, webPodcasts, webNews]) {
            for (let i = m.count - 1; i >= 0; --i) {
                const name = m.nameAt(i)
                const choice = choices.find(c => c.model === m && c.name === name)
                if (choice && !choice.on) m.hide(i)
            }
        }
        radio.setChosen(choices.filter(c => c.model === radio && c.on).map(c => c.name))
        appModel.welcomeDone = true
        visible = false
        console.info("Welcome done:", choices.filter(c => c.on).length, "services")
        done()
        if (returnFocusTo) returnFocusTo.forceActiveFocus()
    }

    Rectangle {
        anchors.fill: parent
        color: "#0b0c0f"
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }
    }

    Column {
        id: head
        x: welcome.unit * 6
        y: welcome.unit * 5
        width: parent.width - welcome.unit * 12
        spacing: welcome.unit * 1.2
        Row {
            spacing: welcome.unit * 1.5
            Image {
                anchors.verticalCenter: parent.verticalCenter
                width: welcome.unit * 6; height: width
                source: "qrc:/assets/kijkluister-256.png"
                sourceSize: Qt.size(128, 128)
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.verticalCenter: parent.verticalCenter
                color: "white"
                font.pixelSize: (welcome.unit * 5) * appModel.fontScale
                font.weight: Font.DemiBold
                text: qsTr("Welkom bij Kijkluister")
            }
        }
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            width: parent.width
            wrapMode: Text.Wrap
            color: "#d5d8dc"
            font.pixelSize: (welcome.unit * 2.8) * appModel.fontScale
            text: qsTr("Welke diensten en radiozenders gebruik je? Vink ze aan. De rest zetten we niet op het startscherm; via Instellingen haal je ze altijd terug.")
        }
    }

    GridView {
        id: grid
        anchors.top: head.bottom
        anchors.topMargin: welcome.unit * 3
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: footer.top
        anchors.leftMargin: welcome.unit * 6
        anchors.rightMargin: welcome.unit * 6
        clip: true
        cellWidth: Math.floor(width / 5)
        cellHeight: welcome.unit * 11
        model: welcome.choices
        keyNavigationEnabled: false
        Keys.onLeftPressed: welcome.step(-1, 0)
        Keys.onRightPressed: welcome.step(1, 0)
        Keys.onUpPressed: welcome.step(0, -1)
        Keys.onReturnPressed: welcome.toggle(currentIndex)
        Keys.onEnterPressed: welcome.toggle(currentIndex)
        Keys.onSpacePressed: welcome.toggle(currentIndex)
        Keys.onDownPressed: (event) => {
            if (!welcome.step(0, 1)) doneButton.forceActiveFocus()
        }
        delegate: Item {
            id: cell
            required property var modelData
            required property int index
            readonly property bool current: GridView.isCurrentItem && grid.activeFocus
            width: grid.cellWidth
            height: grid.cellHeight
            z: cell.modelData.header ? 1 : 0
            // Kopje (Streaming, Muziek, ...) over de hele breedte
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                visible: !!cell.modelData.header
                anchors.left: parent.left
                anchors.leftMargin: welcome.unit * 0.7
                anchors.bottom: parent.bottom
                anchors.bottomMargin: welcome.unit * 1.5
                text: cell.modelData.header || ""
                color: "white"
                font.pixelSize: (welcome.unit * 3.6) * appModel.fontScale
                font.weight: Font.DemiBold
            }
            Rectangle {
                visible: !cell.modelData.header && !cell.modelData.spacer
                anchors.fill: parent
                anchors.margins: welcome.unit * 0.7
                radius: welcome.unit
                color: cell.modelData.on ? cell.modelData.accent : "#1c1e23"
                opacity: cell.modelData.on ? 1 : 0.75
                border.width: cell.current ? Math.max(3, welcome.unit * 0.4) : 0
                border.color: "white"
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    x: welcome.unit * 1.5
                    spacing: welcome.unit * 1.2
                    // Vinkje
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: welcome.unit * 3; height: width; radius: welcome.unit * 0.6
                        color: cell.modelData.on ? "white" : "transparent"
                        border.width: 2
                        border.color: "white"
                        Text {
                            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                            anchors.centerIn: parent
                            visible: cell.modelData.on
                            color: "#202124"
                            font.pixelSize: welcome.unit * 2.4
                            font.weight: Font.Bold
                            text: "✓"
                        }
                    }
                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        width: welcome.unit * 4.5; height: width
                        source: cell.modelData.icon
                        sourceSize: Qt.size(96, 96)
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                            width: grid.cellWidth - welcome.unit * 13
                            elide: Text.ElideRight
                            color: "white"
                            font.pixelSize: (welcome.unit * 2.4) * appModel.fontScale
                            font.weight: Font.DemiBold
                            text: cell.modelData.name
                        }
                        Text {
                            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                            color: "#d5d8dc"
                            font.pixelSize: (welcome.unit * 1.7) * appModel.fontScale
                            text: cell.modelData.kind || ""
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: { grid.currentIndex = cell.index; grid.forceActiveFocus(); welcome.toggle(cell.index) }
                }
            }
        }
    }

    Row {
        id: footer
        anchors.bottom: parent.bottom
        anchors.bottomMargin: welcome.unit * 4
        x: welcome.unit * 6
        spacing: welcome.unit * 3
        FocusButton {
            id: doneButton
            unit: welcome.unit
            text: qsTr("Klaar (") + welcome.choices.filter(c => c.on).length + qsTr(" gekozen)")
            onClicked: welcome.finish()
            KeyNavigation.up: grid
        }
        HintBar {
            anchors.verticalCenter: parent.verticalCenter
            unit: welcome.unit
            items: [
                { pad: "✛", key: qsTr("Pijltjes"), label: qsTr("Kiezen") },
                { pad: "A", key: qsTr("Enter"), label: qsTr("Aan / uit") }
            ]
        }
    }
}
