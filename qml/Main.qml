// PP Kiosk -- startscherm: klok, mededelingen, dagstructuur (nu / straks / hele dag) en tegels.
// Ontworpen voor een 32 inch Full HD touchscreen (1920x1080); schaalt mee op andere schermen.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    color: contrast ? "#000000" : part.bg
    Behavior on color { ColorAnimation { duration: 2000 } }
    focus: true

    readonly property real s: Math.min(width / 1920, height / 1080)
    // Instellingen uit de beheerpagina
    readonly property var cfg: agenda.settings
    readonly property bool contrast: cfg.contrast === true
    readonly property real f: s * (cfg.textScale || 1)          // tekstgrootte (grote-tekst-stand)
    readonly property color ink: contrast ? "#FFFFFF" : "#1F2A44"
    readonly property color soft: contrast ? "#FFE14D" : "#5B6478"
    readonly property color green: contrast ? "#7CFC9A" : "#2E8B57"
    readonly property color card: contrast ? "#000000" : "#FFFFFF"
    readonly property color line: contrast ? "#FFFFFF" : "#14000000"
    readonly property int lineWidth: contrast ? 4 : 2
    function fill(c) { return contrast ? "#000000" : c }
    // Dagdeel: achtergrond en begroeting
    readonly property var dayparts: ({
        ochtend: { bg: "#FFF7E8", hello: "Goedemorgen", icon: "🌅" },
        middag: { bg: "#F7F4EE", hello: "Goedemiddag", icon: "☀️" },
        avond: { bg: "#EEF0F8", hello: "Goedenavond", icon: "🌆" },
        nacht: { bg: "#E6E8F0", hello: "Goedenacht", icon: "🌙" }
    })
    readonly property var part: dayparts[agenda.daypart] || dayparts.middag
    readonly property string emojiFont: Qt.platform.os === "windows" ? "Segoe UI Emoji" : "Noto Color Emoji"
    property date now: new Date()
    property string page: "home"     // home, stream, presentations, shortcuts

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

    // Pictogram van een activiteit: eigen plaatje (picto) of anders de emoji
    component Picto: Item {
        property var it: null
        property real size: 64
        Image {
            anchors.fill: parent
            visible: !!(parent.it && parent.it.imageUrl)
            source: parent.it && parent.it.imageUrl ? parent.it.imageUrl : ""
            fillMode: Image.PreserveAspectFit
            sourceSize.width: 512
            asynchronous: true
            smooth: true
        }
        Text {
            anchors.centerIn: parent
            visible: !(parent.it && parent.it.imageUrl)
            text: parent.it ? (parent.it.icon || "") : ""
            font.family: root.emojiFont
            font.pixelSize: parent.size
        }
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
        height: 150 * root.f

        Row {
            id: clock
            spacing: 32 * root.s
            anchors.verticalCenter: parent.verticalCenter
            Text {
                text: Qt.formatTime(root.now, "HH:mm")
                font.pixelSize: 120 * root.f; font.bold: true; color: root.ink
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    text: root.part.hello + ", het is " + root.now.toLocaleDateString(Qt.locale("nl_NL"), "dddd")
                    font.pixelSize: 52 * root.f; font.bold: true; color: root.green
                }
                Text {
                    text: root.now.toLocaleDateString(Qt.locale("nl_NL"), "d MMMM yyyy")
                    font.pixelSize: 40 * root.f; color: root.soft
                }
            }
            // Lang vasthouden op de klok: beheergegevens tonen (adres, code, QR)
            TapHandler {
                longPressThreshold: 1.5
                onLongPressed: adminPanel.visible = true
            }
        }

        Row {
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            spacing: 20 * root.s
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: weather.available ? weather.icon : root.part.icon
                font.family: root.emojiFont
                font.pixelSize: 96 * root.f
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    visible: weather.available
                    text: weather.temperature + "°"
                    font.pixelSize: 64 * root.f; font.bold: true; color: root.ink
                }
                Text {
                    visible: weather.available
                    text: weather.text
                    font.pixelSize: 34 * root.f; color: root.soft
                }
            }
        }
    }

    // ---------- Mededelingen en nieuws (wisselen om de 9 seconden) ----------
    readonly property var tickerItems: {
        const out = agenda.messages.map(m => ({ kind: "message", title: m, time: "" }))
        if (root.cfg.news !== false)
            for (const h of news.headlines) out.push({ kind: "news", title: h.title, time: h.time })
        return out
    }
    property int tickerIndex: 0
    readonly property var tickerItem: tickerItems.length > 0 ? tickerItems[tickerIndex % tickerItems.length] : null
    Timer {
        interval: 9000; repeat: true; running: root.tickerItems.length > 1
        onTriggered: tickerFade.restart()
    }
    SequentialAnimation {
        id: tickerFade
        NumberAnimation { target: tickerRow; property: "opacity"; to: 0; duration: 350 }
        ScriptAction { script: root.tickerIndex = (root.tickerIndex + 1) % Math.max(1, root.tickerItems.length) }
        NumberAnimation { target: tickerRow; property: "opacity"; to: 1; duration: 350 }
    }
    Rectangle {
        id: messageBar
        anchors { left: parent.left; right: parent.right; top: header.bottom; leftMargin: 48 * root.s; rightMargin: 48 * root.s; topMargin: 8 * root.s }
        visible: root.tickerItem !== null
        readonly property bool isNews: visible && root.tickerItem.kind === "news"
        height: visible ? 84 * root.f : 0
        radius: 20 * root.s
        color: root.contrast ? "#000000" : isNews ? "#1F2A44" : "#FFF4CC"
        border.color: root.contrast ? "#FFE14D" : "transparent"; border.width: root.lineWidth
        Behavior on color { ColorAnimation { duration: 300 } }
        RowLayout {
            id: tickerRow
            anchors { fill: parent; leftMargin: 20 * root.s; rightMargin: 28 * root.s }
            spacing: 20 * root.s
            Rectangle {
                visible: messageBar.isNews
                Layout.preferredHeight: 56 * root.f
                Layout.preferredWidth: newsLabel.implicitWidth + 32 * root.s
                radius: 12 * root.s
                color: "#E1251B"
                Text {
                    id: newsLabel
                    anchors.centerIn: parent
                    text: "NOS NIEUWS"
                    font.pixelSize: 30 * root.f; font.bold: true; font.letterSpacing: 2 * root.s; color: "#FFFFFF"
                }
            }
            Text {
                visible: !messageBar.isNews
                text: "📢"
                font.family: root.emojiFont
                font.pixelSize: 38 * root.f
            }
            Text {
                id: messageText
                Layout.fillWidth: true
                text: root.tickerItem ? root.tickerItem.title : ""
                font.pixelSize: 38 * root.f
                font.bold: messageBar.isNews
                color: root.contrast ? "#FFFFFF" : messageBar.isNews ? "#FFFFFF" : root.ink
                elide: Text.ElideRight
            }
            Text {
                visible: messageBar.isNews && root.tickerItem.time !== ""
                text: root.tickerItem ? root.tickerItem.time : ""
                font.pixelSize: 32 * root.f; color: "#AEB8D6"
            }
        }
    }

    // ---------- Verjaardag of bijzondere dag ----------
    Rectangle {
        id: party
        anchors { left: parent.left; right: parent.right; top: messageBar.visible ? messageBar.bottom : header.bottom; leftMargin: 48 * root.s; rightMargin: 48 * root.s; topMargin: 12 * root.s }
        visible: agenda.celebrations.length > 0
        height: visible ? 96 * root.f : 0
        radius: 24 * root.s
        clip: true
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: root.contrast ? "#000000" : "#FF7EB3" }
            GradientStop { position: 1; color: root.contrast ? "#000000" : "#FFC72C" }
        }
        border.color: root.contrast ? "#FFFFFF" : "transparent"; border.width: root.lineWidth
        // Confetti
        Repeater {
            model: party.visible ? 28 : 0
            delegate: Rectangle {
                required property int index
                width: 14 * root.s; height: 22 * root.s; radius: 3 * root.s
                color: ["#FFFFFF", "#2E8B57", "#5DA9E9", "#DA291C", "#F6C85F", "#C3B1E1"][index % 6]
                x: (index * 137) % Math.max(1, party.width)
                rotation: index * 33
                NumberAnimation on y {
                    from: -30 * root.s; to: party.height + 10 * root.s
                    duration: 2400 + (index % 7) * 500; loops: Animation.Infinite
                }
            }
        }
        Row {
            anchors.centerIn: parent
            spacing: 24 * root.s
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: agenda.celebrations.length ? agenda.celebrations[0].icon : ""
                font.family: root.emojiFont
                font.pixelSize: 60 * root.f
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, party.width - 200 * root.s)
                text: agenda.celebrations.map(c => c.title).join("   •   ")
                font.pixelSize: 56 * root.f; font.bold: true; color: root.contrast ? "#FFE14D" : "#FFFFFF"
                style: Text.Outline; styleColor: root.contrast ? "#000000" : "#55000000"
                elide: Text.ElideRight
            }
        }
    }

    // ---------- Inhoud ----------
    RowLayout {
        anchors { left: parent.left; right: parent.right; top: party.visible ? party.bottom : messageBar.visible ? messageBar.bottom : header.bottom; bottom: parent.bottom; margins: 48 * root.s; topMargin: 24 * root.s }
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
                Layout.minimumHeight: 220 * root.f
                radius: 32 * root.s
                color: root.current ? root.fill(root.current.color) : root.card
                border.color: root.line; border.width: root.lineWidth

                Text {
                    anchors { left: parent.left; top: parent.top; margins: 32 * root.s }
                    text: root.current ? "NU" : "NU GEEN ACTIVITEIT"
                    font.pixelSize: 34 * root.f; font.bold: true; font.letterSpacing: 3 * root.s; color: root.ink; opacity: 0.7
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
                        color: root.contrast ? "#FFFFFF" : "#AAFFFFFF"
                        Picto {
                            anchors { fill: parent; margins: parent.side * 0.1 }
                            it: root.current
                            size: parent.side * 0.66
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 12 * root.s
                        Text {
                            Layout.fillWidth: true
                            text: root.current ? root.current.title : ""
                            font.pixelSize: Math.min(80 * root.f, nowCard.height * 0.26); font.bold: true; color: root.ink
                            wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                        }
                        Text {
                            text: root.current ? root.timeRange(root.current) : ""
                            font.pixelSize: Math.min(52 * root.f, nowCard.height * 0.15); color: root.ink
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: text !== "" && nowCard.height > 380 * root.s
                            text: root.current ? (root.current.note || "") : ""
                            font.pixelSize: 38 * root.f; color: root.ink; opacity: 0.8
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
                TapHandler {
                    onTapped: if (root.cfg.speak && root.current) speaker.say("Nu: " + root.current.title)
                }
                Text {
                    anchors.centerIn: parent
                    visible: root.current === null
                    text: root.next ? "Vrije tijd" : "De dag zit erop 🌙"
                    font.pixelSize: 72 * root.f; color: root.soft
                }
            }

            // STRAKS en VANDAAG ETEN naast elkaar
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 136 * root.f
                Layout.maximumHeight: 136 * root.f
                Layout.minimumHeight: 112 * root.f
                visible: root.next !== null || !!agenda.todayMenu.dish
                spacing: 24 * root.s

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredWidth: 3
                Layout.fillHeight: true
                radius: 28 * root.s
                color: root.card
                border.color: root.line; border.width: root.lineWidth
                visible: root.next !== null
                RowLayout {
                    anchors { fill: parent; leftMargin: 28 * root.s; rightMargin: 28 * root.s }
                    spacing: 22 * root.s
                    Rectangle {
                        Layout.preferredWidth: 104 * root.s; Layout.preferredHeight: 104 * root.s
                        radius: 24 * root.s
                        color: root.next ? (root.contrast ? "#FFFFFF" : root.next.color) : "transparent"
                        Picto {
                            anchors { fill: parent; margins: 12 * root.s }
                            it: root.next
                            size: 76 * root.s
                        }
                    }
                    Column {
                        Layout.fillWidth: true
                        Text {
                            text: "STRAKS  " + (root.next ? root.next.start + "  " + root.untilText(root.next) : "")
                            font.pixelSize: 28 * root.f; font.bold: true; color: root.soft
                            width: parent.width; elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: root.next ? root.next.title : ""
                            font.pixelSize: 50 * root.f; font.bold: true; color: root.ink
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            // VANDAAG ETEN (uit het weekmenu)
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredWidth: 2
                Layout.fillHeight: true
                radius: 28 * root.s
                color: root.card
                border.color: root.line; border.width: root.lineWidth
                visible: !!agenda.todayMenu.dish
                RowLayout {
                    anchors { fill: parent; leftMargin: 24 * root.s; rightMargin: 24 * root.s }
                    spacing: 18 * root.s
                    Picto {
                        Layout.preferredWidth: 84 * root.s; Layout.preferredHeight: 84 * root.s
                        it: ({ icon: "🍽️", imageUrl: agenda.todayMenu.imageUrl })
                        size: 64 * root.s
                    }
                    Column {
                        Layout.fillWidth: true
                        Text {
                            text: "VANDAAG ETEN"
                            font.pixelSize: 28 * root.f; font.bold: true; color: root.soft
                        }
                        Text {
                            width: parent.width
                            text: (agenda.todayMenu.dish || "") + (agenda.todayMenu.note ? " " + agenda.todayMenu.note : "")
                            font.pixelSize: 38 * root.f; font.bold: true; color: root.ink
                            wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                        }
                    }
                }
                TapHandler { onTapped: if (root.cfg.speak) speaker.say("Vandaag eten we " + agenda.todayMenu.dish + " " + (agenda.todayMenu.note || "")) }
            }
            }

            // Tegels naar de andere onderdelen
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.preferredHeight: (root.f > root.s ? 120 : 160) * root.s
                Layout.maximumHeight: (root.f > root.s ? 120 : 160) * root.s
                spacing: 24 * root.s
                Repeater {
                    model: [
                        { title: "Presentaties", icon: "🖼️", tint: "#5DA9E9", page: "presentations" },
                        { title: "Kijk & luister", icon: "🎧", tint: "#6CC28B", page: "stream" },
                        { title: "Snelkoppelingen", icon: "⭐", tint: "#E97A6B", page: "shortcuts" }
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
                                font.pixelSize: (root.f > root.s ? 44 : 56) * root.s
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.title
                                font.pixelSize: 36 * root.f; font.bold: true; color: "#FFFFFF"
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
            color: root.card
            border.color: root.line; border.width: root.lineWidth

            Text {
                id: dayTitle
                anchors { left: parent.left; top: parent.top; margins: 32 * root.s }
                text: "Mijn dag"
                font.pixelSize: 48 * root.f; font.bold: true; color: root.ink
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
                    color: isNow ? root.fill(modelData.color) : "transparent"
                    border.color: modelData.state === "next" ? (root.contrast ? "#FFE14D" : modelData.color) : (isNow && root.contrast ? "#FFFFFF" : "transparent")
                    border.width: 4 * root.s
                    opacity: isPast ? 0.45 : 1

                    RowLayout {
                        anchors { fill: parent; leftMargin: 16 * root.s; rightMargin: 16 * root.s }
                        spacing: 18 * root.s
                        Text {
                            Layout.preferredWidth: 110 * root.s
                            text: modelData.start
                            font.pixelSize: 36 * root.f; font.bold: isNow; color: root.ink
                        }
                        Rectangle {
                            Layout.preferredWidth: 76 * root.s; Layout.preferredHeight: 76 * root.s
                            radius: 18 * root.s
                            color: root.contrast ? "#FFFFFF" : isNow ? "#AAFFFFFF" : modelData.color
                            Picto {
                                anchors { fill: parent; margins: 8 * root.s }
                                it: modelData
                                size: 46 * root.s
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.title
                            font.pixelSize: (isNow ? 40 : 36) * root.f; font.bold: isNow; color: root.ink
                            font.strikeout: isPast
                            elide: Text.ElideRight
                        }
                        Text {
                            visible: isPast
                            text: "✓"
                            font.pixelSize: 40 * root.f; color: root.green
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
                    font.pixelSize: 36 * root.f; color: root.soft
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
        sourceComponent: root.page === "stream" ? streamPage : root.page === "presentations" ? presentationPage
                         : root.page === "shortcuts" ? shortcutsPage : null
    }
    Component { id: streamPage; StreamScreen { s: root.s; onBack: root.page = "home" } }
    Component { id: presentationPage; PresentationScreen { s: root.s; onBack: root.page = "home" } }
    Component { id: shortcutsPage; ShortcutsScreen { s: root.s; onBack: root.page = "home" } }

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
            } else if (selfTest === "shortcuts") {
                root.page = "shortcuts"
            } else if (selfTest === "upload") {
                admin.uploadTest()
            } else if (selfTest === "menu") {
                console.info("Menu today: " + (agenda.todayMenu.dish ? "yes" : "no") + " daypart " + agenda.daypart)
            } else if (selfTest === "speak") {
                speaker.announce("Het is tijd voor: Test", true)
            } else if (selfTest === "news") {
                news.loadSample()
                console.info("News ticker: " + root.tickerItems.length + " items, shown " + (root.tickerItem ? root.tickerItem.title : "-"))
            } else if (selfTest === "menuboard") {
                const board = presentations.list.find(x => x.folder === "Menu van de week")
                console.info("Menu board presentation: " + (board ? board.count : 0) + " slides")
                root.page = "presentations"
                pageLoader.item.startNamed("Menu van de week")
            } else if (selfTest === "radiosaver") {
                root.page = "stream"
                pageLoader.item.section = "radio"
                pageLoader.item.radioRow = 0
                pageLoader.item.saver = true
            } else if (selfTest === "screensaver") {
                root.page = "presentations"
                if (pageLoader.item.startNamed("")) { root.screensaver = true; console.info("Screensaver started") }
            } else if (selfTest === "admin") {
                adminPanel.visible = true
                admin.selfTest()
            }
            console.info("Self test " + selfTest + " ok")
        }
    }

    // ---------- Voorlezen bij het begin van een activiteit ----------
    Connections {
        target: agenda
        function onActivityStarted(title, note) {
            if (root.cfg.speak && !root.night)
                speaker.announce("Het is tijd voor: " + title + (note ? ". " + note : ""), root.cfg.chime !== false)
        }
    }

    // ---------- Screensaver: na een tijd zonder aanraking een presentatie ----------
    property bool screensaver: false
    Timer {
        interval: 5000; running: true; repeat: true
        onTriggered: {
            const min = root.cfg.screensaverMinutes || 0
            // Niet tijdens een presentatie of als de radio speelt (die heeft een eigen screensaver)
            const item = pageLoader.item
            const busy = item && ((item.show !== undefined && item.show !== null) || (item.radioRow !== undefined && item.radioRow >= 0))
            if (min > 0 && !root.screensaver && !root.night && !busy && !adminPanel.visible
                    && activity.idleSeconds() >= min * 60 && presentations.list.length > 0) {
                root.page = "presentations"
                if (pageLoader.item && pageLoader.item.startNamed(root.cfg.screensaverPresentation || "")) {
                    root.screensaver = true
                    console.info("Screensaver started")
                } else {
                    root.page = "home"
                }
            }
        }
    }
    Connections {
        target: activity
        function onTouched() {
            if (root.screensaver) {
                root.screensaver = false
                root.page = "home"
                console.info("Screensaver stopped")
            }
            root.wakeUntil = Date.now() + 2 * 60 * 1000
        }
    }

    // ---------- Nachtstand: scherm dimmen of zwart ----------
    property double wakeUntil: 0
    function inRange(t, a, b) { return a <= b ? (t >= a && t < b) : (t >= a || t < b) }
    readonly property bool night: !!(cfg.nightFrom && cfg.nightTo)
                                   && inRange(Qt.formatTime(now, "HH:mm"), cfg.nightFrom, cfg.nightTo)
                                   && now.getTime() > wakeUntil
    onNightChanged: {
        console.info("Night mode " + night)
        if (night) { root.screensaver = false; root.page = "home" }
    }
    Rectangle {
        anchors.fill: parent
        visible: root.night
        color: "#000000"
        opacity: root.cfg.nightMode === "black" ? 1 : 0.88
        z: 100
        Text {
            anchors.centerIn: parent
            visible: root.cfg.nightMode !== "black"
            text: "🌙  " + Qt.formatTime(root.now, "HH:mm")
            font.family: root.emojiFont
            font.pixelSize: 140 * root.s; color: "#8090B0"
        }
        TapHandler { onTapped: root.wakeUntil = Date.now() + 2 * 60 * 1000 }
    }
}
