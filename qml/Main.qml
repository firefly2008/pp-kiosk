// PP Kiosk -- startscherm: klok, mededelingen, dagstructuur (nu / straks / hele dag) en tegels.
// Ontworpen voor een 32 inch Full HD touchscreen (1920x1080); schaalt mee op andere schermen.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#F7F4EE"
    focus: true

    readonly property real s: Math.min(width / 1920, height / 1080)
    readonly property color ink: "#1F2A44"
    readonly property color soft: "#5B6478"
    readonly property color green: "#2E8B57"
    readonly property string emojiFont: Qt.platform.os === "windows" ? "Segoe UI Emoji" : "Noto Color Emoji"
    property date now: new Date()
    property string page: "home"     // home, stream, presentations

    readonly property var items: agenda.today
    readonly property var current: agenda.currentIndex >= 0 ? items[agenda.currentIndex] : null
    readonly property var next: agenda.nextIndex >= 0 ? items[agenda.nextIndex] : null

    function minutes(t) { return t ? parseInt(t.substr(0, 2)) * 60 + parseInt(t.substr(3, 2)) : -1 }
    function timeRange(it) { return it.end ? it.start + " – " + it.end : it.start }
    function progress(it) {
        if (!it || !it.end) return -1
        const a = minutes(it.start), b = minutes(it.end)
        const n = now.getHours() * 60 + now.getMinutes()
        return b > a ? Math.max(0, Math.min(1, (n - a) / (b - a))) : -1
    }
    function untilText(it) {
        if (!it) return ""
        const d = minutes(it.start) - (now.getHours() * 60 + now.getMinutes())
        if (d <= 0) return ""
        if (d < 60) return "over " + d + (d === 1 ? " minuut" : " minuten")
        const h = Math.floor(d / 60), m = d % 60
        return "over " + h + " uur" + (m ? " en " + m + " min" : "")
    }

    Timer {
        interval: 1000; running: true; repeat: true
        onTriggered: root.now = new Date()
    }

    Component.onCompleted: console.info("Home screen ready; agenda items " + items.length + " now " + agenda.currentIndex + " next " + agenda.nextIndex)

    // ---------- Bovenbalk ----------
    Item {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 48 * root.s; rightMargin: 48 * root.s; topMargin: 24 * root.s }
        height: 150 * root.s

        Row {
            id: clock
            spacing: 32 * root.s
            anchors.verticalCenter: parent.verticalCenter
            Text {
                text: Qt.formatTime(root.now, "HH:mm")
                font.pixelSize: 120 * root.s; font.bold: true; color: root.ink
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    text: root.now.toLocaleDateString(Qt.locale("nl_NL"), "dddd")
                    font.pixelSize: 52 * root.s; font.bold: true; color: root.green
                }
                Text {
                    text: root.now.toLocaleDateString(Qt.locale("nl_NL"), "d MMMM yyyy")
                    font.pixelSize: 40 * root.s; color: root.soft
                }
            }
            // Lang vasthouden op de klok: beheergegevens tonen (adres, code, QR)
            TapHandler {
                longPressThreshold: 1.5
                onLongPressed: adminPanel.visible = true
            }
        }

        Text {
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            visible: weather.available
            text: weather.summary
            font.pixelSize: 48 * root.s; color: root.soft
        }
    }

    // ---------- Mededelingen ----------
    Rectangle {
        id: messageBar
        anchors { left: parent.left; right: parent.right; top: header.bottom; leftMargin: 48 * root.s; rightMargin: 48 * root.s; topMargin: 8 * root.s }
        visible: agenda.messages.length > 0
        height: visible ? messageText.implicitHeight + 32 * root.s : 0
        radius: 20 * root.s
        color: "#FFF4CC"
        Text {
            id: messageText
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 28 * root.s }
            text: "📢  " + agenda.messages.join("     •     ")
            font.pixelSize: 38 * root.s; color: root.ink
            wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
        }
    }

    // ---------- Inhoud ----------
    RowLayout {
        anchors { left: parent.left; right: parent.right; top: messageBar.visible ? messageBar.bottom : header.bottom; bottom: parent.bottom; margins: 48 * root.s; topMargin: 24 * root.s }
        spacing: 32 * root.s

        // Links: nu, straks en de tegels
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 24 * root.s

            // NU
            Rectangle {
                id: nowCard
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 32 * root.s
                color: root.current ? root.current.color : "#FFFFFF"
                border.color: "#14000000"; border.width: 2

                Text {
                    anchors { left: parent.left; top: parent.top; margins: 32 * root.s }
                    text: root.current ? "NU" : "NU GEEN ACTIVITEIT"
                    font.pixelSize: 34 * root.s; font.bold: true; font.letterSpacing: 3 * root.s; color: root.ink; opacity: 0.7
                }
                RowLayout {
                    anchors { fill: parent; margins: 40 * root.s; topMargin: 76 * root.s }
                    spacing: 48 * root.s
                    visible: root.current !== null
                    Rectangle {
                        readonly property real side: Math.min(300 * root.s, nowCard.height - 116 * root.s)
                        Layout.preferredWidth: side
                        Layout.preferredHeight: side
                        Layout.alignment: Qt.AlignVCenter
                        radius: 40 * root.s
                        color: "#AAFFFFFF"
                        Text {
                            anchors.centerIn: parent
                            text: root.current ? root.current.icon : ""
                            font.family: root.emojiFont
                            font.pixelSize: parent.side * 0.66
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 12 * root.s
                        Text {
                            Layout.fillWidth: true
                            text: root.current ? root.current.title : ""
                            font.pixelSize: 80 * root.s; font.bold: true; color: root.ink
                            wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                        }
                        Text {
                            text: root.current ? root.timeRange(root.current) : ""
                            font.pixelSize: 52 * root.s; color: root.ink
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: text !== ""
                            text: root.current ? (root.current.note || "") : ""
                            font.pixelSize: 38 * root.s; color: root.ink; opacity: 0.8
                            wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                        }
                        // Hoe ver is de activiteit (balk vult zich)
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.topMargin: 12 * root.s
                            visible: root.progress(root.current) >= 0
                            Layout.preferredHeight: 24 * root.s
                            radius: height / 2
                            color: "#88FFFFFF"
                            Rectangle {
                                width: parent.width * Math.max(0, root.progress(root.current))
                                height: parent.height; radius: parent.radius
                                color: root.ink; opacity: 0.6
                            }
                        }
                    }
                }
                Text {
                    anchors.centerIn: parent
                    visible: root.current === null
                    text: root.next ? "Vrije tijd" : "De dag zit erop 🌙"
                    font.pixelSize: 72 * root.s; color: root.soft
                }
            }

            // STRAKS
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 160 * root.s
                Layout.maximumHeight: 160 * root.s
                radius: 28 * root.s
                color: "#FFFFFF"
                border.color: "#14000000"; border.width: 2
                visible: root.next !== null
                RowLayout {
                    anchors { fill: parent; leftMargin: 32 * root.s; rightMargin: 32 * root.s }
                    spacing: 28 * root.s
                    Text {
                        text: "STRAKS"
                        font.pixelSize: 30 * root.s; font.bold: true; font.letterSpacing: 3 * root.s; color: root.soft
                    }
                    Rectangle {
                        Layout.preferredWidth: 120 * root.s; Layout.preferredHeight: 120 * root.s
                        radius: 24 * root.s
                        color: root.next ? root.next.color : "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: root.next ? root.next.icon : ""
                            font.family: root.emojiFont
                            font.pixelSize: 76 * root.s
                        }
                    }
                    Column {
                        Layout.fillWidth: true
                        Text {
                            width: parent.width
                            text: root.next ? root.next.title : ""
                            font.pixelSize: 52 * root.s; font.bold: true; color: root.ink
                            elide: Text.ElideRight
                        }
                        Text {
                            text: root.next ? root.next.start + "   " + root.untilText(root.next) : ""
                            font.pixelSize: 36 * root.s; color: root.soft
                        }
                    }
                }
            }

            // Tegels naar de andere onderdelen
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.preferredHeight: 160 * root.s
                Layout.maximumHeight: 160 * root.s
                spacing: 24 * root.s
                Repeater {
                    model: [
                        { title: "Presentaties", icon: "🖼️", tint: "#5DA9E9", page: "presentations" },
                        { title: "Kijk & luister", icon: "🎧", tint: "#6CC28B", page: "stream" },
                        { title: "Snelkoppelingen", icon: "⭐", tint: "#E97A6B", page: "" }
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 28 * root.s
                        color: tap.pressed ? Qt.darker(modelData.tint, 1.15) : modelData.tint
                        Column {
                            anchors.centerIn: parent
                            spacing: 4 * root.s
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.icon
                                font.family: root.emojiFont
                                font.pixelSize: 56 * root.s
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.title
                                font.pixelSize: 36 * root.s; font.bold: true; color: "#FFFFFF"
                            }
                        }
                        TapHandler {
                            id: tap
                            onTapped: {
                                console.info("Tile tapped: " + modelData.title)
                                if (modelData.page !== "") root.page = modelData.page
                            }
                        }
                    }
                }
            }
        }

        // Rechts: de hele dag
        Rectangle {
            Layout.preferredWidth: 620 * root.s
            Layout.fillHeight: true
            radius: 32 * root.s
            color: "#FFFFFF"
            border.color: "#14000000"; border.width: 2

            Text {
                id: dayTitle
                anchors { left: parent.left; top: parent.top; margins: 32 * root.s }
                text: "Mijn dag"
                font.pixelSize: 48 * root.s; font.bold: true; color: root.ink
            }

            ListView {
                id: dayList
                anchors { left: parent.left; right: parent.right; top: dayTitle.bottom; bottom: parent.bottom; margins: 20 * root.s; topMargin: 16 * root.s }
                clip: true
                spacing: 10 * root.s
                model: root.items
                boundsBehavior: Flickable.StopAtBounds
                // Huidige (of volgende) activiteit in beeld houden
                readonly property int focusRow: agenda.currentIndex >= 0 ? agenda.currentIndex : agenda.nextIndex
                onFocusRowChanged: if (focusRow >= 0) positionViewAtIndex(Math.max(0, focusRow - 1), ListView.Beginning)
                onCountChanged: if (focusRow >= 0) positionViewAtIndex(Math.max(0, focusRow - 1), ListView.Beginning)

                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    readonly property bool isNow: modelData.state === "now"
                    readonly property bool isPast: modelData.state === "past"
                    width: ListView.view.width
                    height: (isNow ? 130 : 104) * root.s
                    radius: 22 * root.s
                    color: isNow ? modelData.color : "transparent"
                    border.color: modelData.state === "next" ? modelData.color : "transparent"
                    border.width: 4 * root.s
                    opacity: isPast ? 0.45 : 1

                    RowLayout {
                        anchors { fill: parent; leftMargin: 16 * root.s; rightMargin: 16 * root.s }
                        spacing: 18 * root.s
                        Text {
                            Layout.preferredWidth: 110 * root.s
                            text: modelData.start
                            font.pixelSize: 36 * root.s; font.bold: isNow; color: root.ink
                        }
                        Rectangle {
                            Layout.preferredWidth: 76 * root.s; Layout.preferredHeight: 76 * root.s
                            radius: 18 * root.s
                            color: isNow ? "#AAFFFFFF" : modelData.color
                            Text {
                                anchors.centerIn: parent
                                text: modelData.icon
                                font.family: root.emojiFont
                                font.pixelSize: 46 * root.s
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.title
                            font.pixelSize: (isNow ? 40 : 36) * root.s; font.bold: isNow; color: root.ink
                            font.strikeout: isPast
                            elide: Text.ElideRight
                        }
                        Text {
                            visible: isPast
                            text: "✓"
                            font.pixelSize: 40 * root.s; color: root.green
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: dayList.count === 0
                    width: parent.width - 40 * root.s
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: "Vandaag staat er niets op de planning."
                    font.pixelSize: 36 * root.s; color: root.soft
                }
            }
        }
    }

    // ---------- Beheergegevens (lang op de klok drukken) ----------
    Rectangle {
        id: adminPanel
        anchors.fill: parent
        visible: false
        color: "#CC1F2A44"
        onVisibleChanged: if (visible) closeTimer.restart()
        Timer { id: closeTimer; interval: 120000; onTriggered: adminPanel.visible = false }
        TapHandler { onTapped: adminPanel.visible = false }

        Rectangle {
            anchors.centerIn: parent
            width: 1100 * root.s; height: 620 * root.s
            radius: 32 * root.s
            color: "#FFFFFF"
            TapHandler { }   // tikken op de kaart sluit niet

            RowLayout {
                anchors { fill: parent; margins: 48 * root.s }
                spacing: 48 * root.s
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 16 * root.s
                    Text { text: "Beheerpagina"; font.pixelSize: 56 * root.s; font.bold: true; color: root.ink }
                    Text {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                        text: "Op deze pc: open de browser en ga naar"
                        font.pixelSize: 32 * root.s; color: root.soft
                    }
                    Text { text: "localhost:" + admin.address.split(":").pop(); font.pixelSize: 40 * root.s; font.bold: true; color: root.ink }
                    Text {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                        text: "Op een andere pc, tablet of telefoon (zelfde netwerk): scan de QR-code of ga naar"
                        font.pixelSize: 32 * root.s; color: root.soft
                    }
                    Text { text: admin.address.replace("http://", ""); font.pixelSize: 40 * root.s; font.bold: true; color: root.ink }
                    Text { text: "Code: " + admin.code; font.pixelSize: 48 * root.s; font.bold: true; font.letterSpacing: 6 * root.s; color: root.green }
                    Item { Layout.fillHeight: true }
                    Row {
                        spacing: 20 * root.s
                        Rectangle {
                            width: 300 * root.s; height: 90 * root.s; radius: 20 * root.s
                            color: newCodeTap.pressed ? "#DDE3EA" : "#EEF1F5"
                            Text { anchors.centerIn: parent; text: "Nieuwe code"; font.pixelSize: 34 * root.s; color: root.ink }
                            TapHandler { id: newCodeTap; onTapped: admin.newCode() }
                        }
                        Rectangle {
                            width: 240 * root.s; height: 90 * root.s; radius: 20 * root.s
                            color: closeTap.pressed ? Qt.darker(root.green, 1.2) : root.green
                            Text { anchors.centerIn: parent; text: "Sluiten"; font.pixelSize: 34 * root.s; color: "#FFFFFF" }
                            TapHandler { id: closeTap; onTapped: adminPanel.visible = false }
                        }
                    }
                }
                Image {
                    Layout.preferredWidth: 380 * root.s
                    Layout.preferredHeight: 380 * root.s
                    source: admin.qrImage
                    fillMode: Image.PreserveAspectFit
                    cache: false
                }
            }
        }
    }

    // ---------- Andere pagina's (over het startscherm heen) ----------
    Loader {
        id: pageLoader
        anchors.fill: parent
        active: root.page !== "home"
        sourceComponent: root.page === "stream" ? streamPage : root.page === "presentations" ? presentationPage : null
    }
    Component { id: streamPage; StreamScreen { s: root.s; onBack: root.page = "home" } }
    Component { id: presentationPage; PresentationScreen { s: root.s; onBack: root.page = "home" } }

    // Na 15 minuten terug naar het startscherm (behalve tijdens een presentatie of radio)
    Timer {
        id: idle
        interval: 15 * 60 * 1000
        running: root.page !== "home"
        onTriggered: {
            const busy = pageLoader.item && ((pageLoader.item.show !== undefined && pageLoader.item.show !== null)
                                            || (pageLoader.item.radioRow !== undefined && pageLoader.item.radioRow >= 0))
            if (busy) restart(); else root.page = "home"
        }
    }

    Keys.onEscapePressed: {
        if (adminPanel.visible) adminPanel.visible = false
        else if (root.page !== "home") root.page = "home"
        else if (selfTest !== "") Qt.quit()
    }

    // ---------- Rooktest ----------
    Timer {
        running: selfTest !== ""
        interval: 3000
        onTriggered: {
            if (selfTest === "agenda") {
                agenda.setTestTime("10:30")
                console.info("Agenda test: now " + agenda.currentIndex + " title " + (root.current ? root.current.title : "-") + " next " + (root.next ? root.next.title : "-"))
            } else if (selfTest === "stream") {
                root.page = "stream"
            } else if (selfTest === "presentation") {
                root.page = "presentations"
                console.info("Presentations listed: " + presentations.list.length)
                if (presentations.list.length > 0) pageLoader.item.start(presentations.list[0])
            } else if (selfTest === "admin") {
                adminPanel.visible = true
                admin.selfTest()
            }
            console.info("Self test " + selfTest + " ok")
        }
    }
}
