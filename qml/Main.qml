// PP Kiosk -- startscherm: klok, mededelingen, dagstructuur (nu / straks / hele dag) en tegels.
// Ontworpen voor een 32 inch Full HD touchscreen (1920x1080); schaalt mee op andere schermen.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts
import QtMultimedia

Rectangle {
    id: root
    color: contrast ? "#000000" : bgType === "color" ? bgColor : glass ? "#0A0F1E" : part.bg
    Behavior on color { ColorAnimation { duration: 2000 } }
    focus: true

    readonly property real s: Math.min(width / 1920, height / 1080)
    // Instellingen uit de beheerpagina
    readonly property var cfg: agenda.settings
    readonly property bool contrast: cfg.contrast === true
    // Volwassen thema: rustige kleuren (donkerblauw, zand, salie), minder ronde hoeken, lijn-pictogrammen
    readonly property bool adult: cfg.theme === "volwassen" && !contrast
    // Glas-thema: donker, doorschijnende glaskaarten, zachte gloed, grote letters en pill-knoppen
    readonly property bool glass: cfg.theme === "glas" && !contrast
    // Extra wensen 0.11 (beheer: Scherm > Startscherm / Uiterlijk)
    readonly property bool analog: cfg.clockStyle === "analoog"
    readonly property bool dayBig: cfg.dayBig === true
    readonly property real big: cfg.bigButtons === true ? 1.35 : 1          // grote knoppen
    readonly property bool tapSpeak: cfg.readAloud !== false               // voorlezen bij tikken
    Binding { target: news; property: "offline"; value: root.cfg.offline === true }
    Binding { target: weather; property: "offline"; value: root.cfg.offline === true }
    Binding { target: watchdog; property: "enabled"; value: root.cfg.watchdog !== false }
    Binding { target: radio; property: "region"; value: root.cfg.radioRegion || "" }
    readonly property var season: {
        const md = (now.getMonth() + 1) * 100 + now.getDate()
        if (md >= 321 && md < 621) return { name: "lente", icon: "🌷", key: "lente" }
        if (md >= 621 && md < 923) return { name: "zomer", icon: "☀️", key: "zomer" }
        if (md >= 923 && md < 1221) return { name: "herfst", icon: "🍂", key: "herfst" }
        return { name: "winter", icon: "❄️", key: "winter" }
    }
    // Hoe doorzichtig de glaskaarten zijn (beheer: 50-95%, standaard 80%)
    readonly property real glassAlpha: 1 - Math.max(50, Math.min(95, cfg.glassTransparency || 80)) / 100
    readonly property var bg: cfg.background || {}
    readonly property string bgType: bg.type || "daypart"
    readonly property color bgColor: bg.color || "#ECE6DA"
    readonly property string bgPhoto: bgType === "photo" || bgType === "daily" ? (cfg.backgroundUrl || "") : ""
    readonly property real f: s * (cfg.textScale || 1)          // tekstgrootte (grote-tekst-stand)
    readonly property color ink: contrast ? "#FFFFFF" : glass ? "#FFFFFF" : adult ? "#1E2A3A" : "#1F2A44"
    readonly property color soft: contrast ? "#FFE14D" : glass ? "#C3CAE0" : adult ? "#55606E" : "#5B6478"
    readonly property color green: contrast ? "#7CFC9A" : glass ? "#8BE9C0" : adult ? "#4F6E5B" : "#2E8B57"
    readonly property color card: contrast ? "#000000" : glass ? Qt.rgba(1, 1, 1, glassAlpha) : adult ? "#FBF9F5" : "#FFFFFF"
    readonly property color line: contrast ? "#FFFFFF" : glass ? "#40FFFFFF" : adult ? "#22000000" : "#14000000"
    readonly property int lineWidth: contrast ? 4 : 2
    // Hoekafronding: in het volwassen thema strakker
    function r(px) { return px * s * (adult ? 0.35 : glass ? 1.15 : 1) }
    // Kleur van een activiteit; in het volwassen thema gedempt
    function tone(c) {
        if ((!adult && !glass) || !c) return c
        const k = Qt.darker(c, 1.0)
        if (glass) return Qt.rgba(k.r, k.g, k.b, glassAlpha + 0.14)       // gekleurd glas
        return Qt.hsla(k.hslHue, k.hslSaturation * 0.4, Math.min(0.9, k.hslLightness * 0.92 + 0.06), k.a)
    }
    function fill(c) { return contrast ? "#000000" : tone(c) }
    // Dagdeel: achtergrond en begroeting
    readonly property var dayparts: adult ? ({
        ochtend: { bg: "#EFE9DD", hello: "Goedemorgen", icon: "🌅" },
        middag: { bg: "#ECE6DA", hello: "Goedemiddag", icon: "☀️" },
        avond: { bg: "#E3E4E1", hello: "Goedenavond", icon: "🌆" },
        nacht: { bg: "#DCDEE0", hello: "Goedenacht", icon: "🌙" }
    }) : ({
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

    // Ronde knop voor het radiovolume (- en +)
    component VolButton: Rectangle {
        property string label
        signal hit()
        width: 100 * root.s; height: 100 * root.s; radius: width / 2
        color: vt.pressed ? Qt.darker("#2E8B57", 1.2) : "#2E8B57"
        Text { anchors.centerIn: parent; text: parent.label; font.pixelSize: 60 * root.s; font.bold: true; color: "#FFFFFF" }
        TapHandler { id: vt; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: parent.hit() }
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

    Component.onCompleted: {
        console.info("Home screen ready; agenda items " + items.length + " now " + agenda.currentIndex + " next " + agenda.nextIndex)
        if (cfg.voice === true && selfTest === "") listener.prepare()
    }

    // ---------- Eigen achtergrondfoto (met een lichte laag erover, zodat de tekst leesbaar blijft) ----------
    Image {
        id: bgImage
        anchors.fill: parent
        visible: root.bgPhoto !== "" && !root.contrast && status === Image.Ready
        source: root.bgPhoto
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        sourceSize.width: 1920
        onStatusChanged: if (status === Image.Ready) console.info("Background photo shown")
    }
    Rectangle {
        anchors.fill: parent
        visible: bgImage.visible
        color: root.glass ? "#0A0F1E" : root.part.bg
        opacity: root.glass ? 0.62 : 0.72
    }
    // Naam van de fotograaf bij een Unsplash-foto (zo vraagt Unsplash het)
    Text {
        anchors { right: parent.right; bottom: parent.bottom; rightMargin: 16 * root.s; bottomMargin: 4 * root.s }
        visible: bgImage.visible && !!root.bg.credit
        text: "Foto: " + (root.bg.credit || "") + (root.bg.credit === "Unsplash" ? "" : " / Unsplash")
        font.pixelSize: 18 * root.s; color: root.glass ? "#B0FFFFFF" : "#99000000"
    }
    // Glas-thema: zachte, lichtgevende kleurvlekken die heel langzaam bewegen
    GlassBackground {
        id: glow
        anchors.fill: parent
        visible: root.glass && root.bgType === "daypart"
        animate: visible && root.page === "home"
    }

    // ---------- Bovenbalk ----------
    Item {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 48 * root.s; rightMargin: 48 * root.s; topMargin: 24 * root.s }
        height: (root.analog ? 200 : root.dayBig ? 170 : 150) * root.f

        Row {
            id: clock
            spacing: 32 * root.s
            anchors.verticalCenter: parent.verticalCenter
            Text {
                visible: !root.analog
                text: Qt.formatTime(root.now, "HH:mm")
                font.pixelSize: (root.glass ? 136 : 120) * root.f; font.bold: true; color: root.ink
                font.letterSpacing: root.glass ? -3 * root.s : 0
            }
            // Grote analoge klok (beheer: Startscherm > Klok)
            AnalogClock {
                visible: root.analog
                anchors.verticalCenter: parent.verticalCenter
                width: 190 * root.f; height: width
                now: root.now
                glass: root.glass || root.contrast
                ink: root.contrast ? "#FFFFFF" : root.glass ? "#FFFFFF" : root.ink
                accent: root.contrast ? "#FFE14D" : root.green
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    text: root.part.hello + ", het is " + root.now.toLocaleDateString(Qt.locale("nl_NL"), "dddd")
                    font.pixelSize: (root.dayBig ? 66 : 52) * root.f; font.bold: true; color: root.green
                }
                Text {
                    text: root.now.toLocaleDateString(Qt.locale("nl_NL"), "d MMMM yyyy")
                          + (root.dayBig ? "  ·  " + root.season.name + " " + root.season.icon : "")
                          + (root.analog ? "  ·  " + Qt.formatTime(root.now, "HH:mm") + " uur" : "")
                    font.pixelSize: (root.dayBig ? 50 : 40) * root.f; color: root.soft
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
            // Spraakbediening: tik en zeg een opdracht
            Rectangle {
                id: micButton
                anchors.verticalCenter: parent.verticalCenter
                visible: root.cfg.voice === true
                width: 88 * root.f * root.big; height: width
                radius: width / 2
                readonly property bool busy: listener.state === "listening" || listener.state === "starting"
                color: root.contrast ? "#000000" : busy ? "#E74C3C" : (micTap.pressed ? "#DDE3EA" : root.glass ? "#26FFFFFF" : "#FFFFFF")
                border.color: root.contrast ? "#FFE14D" : root.glass ? "#55FFFFFF" : busy ? "transparent" : "#2E8B57"
                border.width: 3 * root.s
                SequentialAnimation on scale {
                    running: micButton.busy; loops: Animation.Infinite
                    NumberAnimation { to: 1.1; duration: 500; easing.type: Easing.InOutQuad }
                    NumberAnimation { to: 1.0; duration: 500; easing.type: Easing.InOutQuad }
                    onRunningChanged: if (!running) micButton.scale = 1
                }
                Text {
                    anchors.centerIn: parent
                    text: "🎤"
                    font.family: root.emojiFont
                    font.pixelSize: 44 * root.f
                }
                TapHandler {
                    gesturePolicy: TapHandler.ReleaseWithinBounds   // knop: tik gaat niet door naar wat eronder ligt
                    id: micTap
                    onTapped: {
                        root.voiceAsked = true
                        if (listener.state === "error" && listener.message !== "") root.voiceReply(listener.message, false)
                        else root.voiceReply(micButton.busy ? "" : "Ik luister…", false)
                        listener.listen()
                    }
                }
            }
            // Muziek: zender van dit dagdeel aanzetten, of de radio die speelt stoppen
            Rectangle {
                id: musicChip
                anchors.verticalCenter: parent.verticalCenter
                visible: root.radioRow >= 0 || root.daypartStation !== ""
                width: musicRow.implicitWidth + 48 * root.s
                height: 88 * root.f * root.big
                radius: height / 2
                color: root.contrast ? "#000000" : root.radioRow >= 0 ? (musicTap.pressed ? "#245F3E" : "#2E8B57") : (musicTap.pressed ? "#DDE3EA" : root.glass ? "#26FFFFFF" : "#FFFFFF")
                border.color: root.contrast ? "#FFE14D" : root.glass ? "#55FFFFFF" : root.radioRow >= 0 ? "transparent" : "#2E8B57"
                border.width: 3 * root.s
                Row {
                    id: musicRow
                    anchors.centerIn: parent
                    spacing: 14 * root.s
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.radioRow >= 0 ? "■" : "🎵"
                        font.family: root.radioRow >= 0 ? "" : root.emojiFont
                        font.pixelSize: 40 * root.f
                        color: root.radioRow >= 0 || root.contrast ? "#FFFFFF" : root.green
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.radioRow >= 0 ? root.radioName : root.daypartStation
                        font.pixelSize: 36 * root.f; font.bold: true
                        color: root.radioRow >= 0 || root.contrast ? "#FFFFFF" : root.green
                        elide: Text.ElideRight
                        width: Math.min(implicitWidth, 360 * root.s)
                    }
                }
                TapHandler {
                    gesturePolicy: TapHandler.ReleaseWithinBounds   // knop: tik gaat niet door naar wat eronder ligt
                    id: musicTap
                    onTapped: {
                        if (root.radioRow >= 0) root.stopRadio(); else root.playDaypart()
                        console.info("Music chip tapped")
                    }
                }
            }
            // Volume van de radio: tik en kies harder of zachter
            Rectangle {
                id: volumeButton
                anchors.verticalCenter: parent.verticalCenter
                visible: root.radioRow >= 0
                width: 88 * root.f * root.big; height: width
                radius: width / 2
                color: root.contrast ? "#000000" : (volumeTap.pressed || volumePanel.visible) ? (root.glass ? "#40FFFFFF" : "#DDE3EA") : root.glass ? "#26FFFFFF" : "#FFFFFF"
                border.color: root.contrast ? "#FFE14D" : root.glass ? "#55FFFFFF" : "#2E8B57"
                border.width: 3 * root.s
                Text {
                    anchors.centerIn: parent
                    text: root.radioVolume <= 0 ? "🔇" : root.radioVolume < 0.5 ? "🔉" : "🔊"
                    font.family: root.emojiFont
                    font.pixelSize: 42 * root.f
                }
                TapHandler {
                    gesturePolicy: TapHandler.ReleaseWithinBounds
                    id: volumeTap
                    onTapped: { volumePanel.visible = !volumePanel.visible; volumeHide.restart() }
                }
            }
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
    property int newsIndex: 0
    function openNews(it) {
        const i = it ? news.headlines.findIndex(h => h.title === it.title) : 0
        newsIndex = Math.max(0, i)
        stats.count("tile", "Nieuws")
        root.page = "news"
    }
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
        radius: root.r(20)
        color: root.contrast ? "#000000" : root.glass ? (isNews ? "#33000000" : "#33FFD970") : isNews ? "#1F2A44" : root.adult ? "#E8DFC8" : "#FFF4CC"
        border.color: root.contrast ? "#FFE14D" : "transparent"; border.width: root.lineWidth
        Behavior on color { ColorAnimation { duration: 300 } }
        // Tik op een nieuwsbericht: het hele bericht lezen op de nieuwspagina
        TapHandler {
            gesturePolicy: TapHandler.ReleaseWithinBounds
            enabled: messageBar.isNews
            onTapped: root.openNews(root.tickerItem)
        }
        RowLayout {
            id: tickerRow
            anchors { fill: parent; leftMargin: 20 * root.s; rightMargin: 28 * root.s }
            spacing: 20 * root.s
            Rectangle {
                visible: messageBar.isNews
                Layout.preferredHeight: 56 * root.f
                Layout.preferredWidth: newsLabel.implicitWidth + 32 * root.s
                radius: root.r(12)
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
                visible: messageBar.isNews
                text: "Lees ›"
                font.pixelSize: 30 * root.f; font.bold: true; color: "#8BE9C0"
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
        radius: root.r(24)
        clip: true
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: root.contrast ? "#000000" : root.adult ? "#8A6F7E" : "#FF7EB3" }
            GradientStop { position: 1; color: root.contrast ? "#000000" : root.adult ? "#B89A5E" : "#FFC72C" }
        }
        border.color: root.contrast ? "#FFFFFF" : "transparent"; border.width: root.lineWidth
        // Confetti (beheer: Scherm > Startscherm > Feestanimatie)
        Repeater {
            model: party.visible && root.cfg.party !== false ? 28 : 0
            delegate: Rectangle {
                required property int index
                width: 14 * root.s; height: 22 * root.s; radius: root.r(3)
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

    readonly property bool birthday: agenda.celebrations.some(c => c.kind === "birthday")
    function celebrate() {
        if (root.cfg.party === false || root.night) return
        console.info("Party: balloons")
        balloons.running = true
        balloonsStop.restart()
    }
    // Verjaardag: ballonnen bij het opstarten, elk heel uur overdag, en bij tikken op de feestbalk
    Timer {
        interval: 30000; repeat: true; running: root.birthday && root.cfg.party !== false
        onTriggered: if (root.now.getMinutes() === 0 && root.now.getSeconds() < 30 && root.now.getHours() >= 8 && root.now.getHours() <= 20) root.celebrate()
    }
    onBirthdayChanged: if (birthday) celebrate()
    TapHandler {
        parent: party
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: {
            root.celebrate()
            if (root.tapSpeak) speaker.say(agenda.celebrations.map(c => c.title).join(". "))
        }
    }
    Timer { id: balloonsStop; interval: 14000; onTriggered: balloons.running = false }
    Item {
        id: balloons
        property bool running: false
        anchors.fill: parent
        z: 90
        visible: running
        Repeater {
            model: balloons.running ? 22 : 0
            delegate: Item {
                id: bal
                required property int index
                width: (90 + (index % 4) * 18) * root.s; height: width * 1.25
                property real sway: 0
                x: ((index * 397) % 100) / 100 * (balloons.width - width) + sway
                y: balloons.height
                Rectangle {
                    width: parent.width; height: parent.width * 1.15
                    radius: width / 2
                    color: ["#FF5E7E", "#FFC72C", "#5DA9E9", "#6CC28B", "#B48AD6", "#FF8A3D"][bal.index % 6]
                    opacity: 0.92
                    Rectangle { x: parent.width * 0.22; y: parent.height * 0.18; width: parent.width * 0.2; height: parent.height * 0.25; radius: width / 2; color: "#66FFFFFF" }
                }
                Rectangle { x: parent.width / 2 - 1; y: parent.width * 1.15; width: 2; height: parent.height * 0.6; color: "#99FFFFFF" }
                NumberAnimation on y {
                    from: balloons.height + 50 * root.s; to: -bal.height * 1.5
                    duration: 7000 + (bal.index % 5) * 1400
                    running: balloons.running
                }
                SequentialAnimation on sway {
                    loops: Animation.Infinite; running: balloons.running
                    NumberAnimation { from: -40 * root.s; to: 40 * root.s; duration: 1600 + bal.index * 60; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 40 * root.s; to: -40 * root.s; duration: 1600 + bal.index * 60; easing.type: Easing.InOutSine }
                }
            }
        }
        Text {
            anchors.centerIn: parent
            text: "🎉"
            font.family: root.emojiFont
            font.pixelSize: 260 * root.s
            NumberAnimation on scale { running: balloons.running; from: 0.4; to: 1.2; duration: 1200; easing.type: Easing.OutBack }
            NumberAnimation on opacity { running: balloons.running; from: 1; to: 0; duration: 4000; easing.type: Easing.InQuad }
        }
    }

    // ---------- Seizoensthema: rustig vallende blaadjes, sneeuw of bloemen ----------
    SeasonDecor {
        anchors.fill: parent
        visible: root.cfg.seasonDecor === true && root.page === "home" && !root.contrast && !root.night
        theme: {
            const m = root.now.getMonth() + 1, d = root.now.getDate()
            if (m === 11 && d >= 14 || m === 12 && d <= 5) return "sinterklaas"
            if (m === 12 && d >= 6) return "kerst"
            return root.season.key
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
                radius: root.r(32)
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
                        radius: root.r(40)
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
                    gesturePolicy: TapHandler.ReleaseWithinBounds   // knop: tik gaat niet door naar wat eronder ligt
                    onTapped: if (root.tapSpeak && root.current) speaker.say("Nu: " + root.current.title + (root.current.note ? ". " + root.current.note : ""))
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
                radius: root.r(28)
                color: root.card
                border.color: root.line; border.width: root.lineWidth
                visible: root.next !== null
                RowLayout {
                    anchors { fill: parent; leftMargin: 28 * root.s; rightMargin: 28 * root.s }
                    spacing: 22 * root.s
                    Rectangle {
                        Layout.preferredWidth: 104 * root.s; Layout.preferredHeight: 104 * root.s
                        radius: root.r(24)
                        color: root.next ? (root.contrast ? "#FFFFFF" : root.tone(root.next.color)) : "transparent"
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
                radius: root.r(28)
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
                TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: if (root.tapSpeak) speaker.say("Vandaag eten we " + agenda.todayMenu.dish + " " + (agenda.todayMenu.note || "")) }
            }
            }

            // Tegels naar de andere onderdelen
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.preferredHeight: (root.f > root.s ? 120 : 160) * root.s * root.big
                Layout.maximumHeight: (root.f > root.s ? 120 : 160) * root.s * root.big
                spacing: 24 * root.s
                Repeater {
                    model: [
                        { title: "Presentaties", icon: "🖼️", line: "presentaties", tint: "#5DA9E9", calm: "#4D6A86", page: "presentations" },
                        { title: "Kijk & luister", icon: "🎧", line: "stream", tint: "#6CC28B", calm: "#5E7D6A", page: "stream" },
                        { title: "Snelkoppelingen", icon: "⭐", line: "snelkoppelingen", tint: "#E97A6B", calm: "#A0745E", page: "shortcuts" },
                        { title: "Spelletjes", icon: "🧩", line: "spelletjes", tint: "#B48AD6", calm: "#6E6684", page: "games" },
                        { title: "Ontspanning", icon: "🌿", line: "", tint: "#E9A23B", calm: "#8A7350", page: "relax" },
                        { title: "Praatkaart", icon: "🗨️", line: "", tint: "#D6457A", calm: "#8A5A6E", page: "talk" },
                        { title: "Onderweg", icon: "🚌", line: "", tint: "#3F7FC0", calm: "#4D6A86", page: "travel" },
                        { title: weather.available ? "Weer " + weather.temperature + "°" : "Weer", icon: weather.available ? weather.icon : "⛅", line: "", tint: "#4FB3C9", calm: "#4F7A86", page: "weather" }
                    ].filter(t => (t.page !== "games" || root.cfg.games !== false) && (t.page !== "weather" || root.cfg.weatherTile === true) && (t.page !== "relax" || root.cfg.relax !== false)
                              && (t.page !== "talk" || root.cfg.talkTile === true) && (t.page !== "travel" || root.cfg.travelTile === true)
                              && (t.page !== "presentations" || root.cfg.tilePres !== false) && (t.page !== "stream" || root.cfg.tileStream !== false)
                              && (t.page !== "shortcuts" || root.cfg.tileLinks !== false))
                    delegate: Rectangle {
                        id: tileBox
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: root.r(28)
                        readonly property color tint: root.adult ? modelData.calm : root.glass ? root.tone(modelData.tint) : modelData.tint
                        color: tap.pressed ? (root.glass ? Qt.rgba(1, 1, 1, 0.3) : Qt.darker(tint, 1.15)) : tint
                        border.color: root.glass ? "#55FFFFFF" : "transparent"
                        border.width: root.glass ? 2 : 0
                        // Glas: lichte rand bovenaan (lichtbreking)
                        Rectangle {
                            visible: root.glass
                            anchors { left: parent.left; right: parent.right; top: parent.top; margins: parent.radius * 0.6 }
                            anchors.topMargin: 3 * root.s
                            height: 2 * root.s
                            color: "#80FFFFFF"
                        }
                        Column {
                            anchors.centerIn: parent
                            spacing: 4 * root.s
                            Image {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: (root.adult || root.glass) && modelData.line !== ""
                                width: (root.f > root.s ? 48 : 60) * root.s * root.big; height: width
                                source: (root.adult || root.glass) && modelData.line !== "" ? "qrc:/icons/" + modelData.line + ".png" : ""
                                smooth: true; mipmap: true
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: (!root.adult && !root.glass) || modelData.line === ""
                                text: modelData.icon
                                font.family: root.emojiFont
                                font.pixelSize: (root.f > root.s ? 44 : 56) * root.s
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: tileBox.width - 24 * root.s
                                horizontalAlignment: Text.AlignHCenter
                                text: modelData.title
                                font.pixelSize: 36 * root.f; font.bold: true; color: "#FFFFFF"
                                fontSizeMode: Text.HorizontalFit; minimumPixelSize: 20 * root.s
                            }
                        }
                        TapHandler {
                            gesturePolicy: TapHandler.ReleaseWithinBounds   // knop: tik gaat niet door naar wat eronder ligt
                            id: tap
                            onTapped: {
                                console.info("Tile tapped: " + modelData.title)
                                stats.count("tile", modelData.title)
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
            radius: root.r(32)
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
                    radius: root.r(22)
                    color: isNow ? root.fill(modelData.color) : "transparent"
                    border.color: modelData.state === "next" ? (root.contrast ? "#FFE14D" : root.tone(modelData.color)) : (isNow && root.contrast ? "#FFFFFF" : "transparent")
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
                            radius: root.r(18)
                            color: root.contrast ? "#FFFFFF" : isNow ? "#AAFFFFFF" : root.tone(modelData.color)
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
                    // Tik op een regel: voorlezen (beheer: Dagstructuur > Herinneringen > Voorlezen bij tikken)
                    TapHandler {
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onTapped: if (root.tapSpeak) speaker.say("Om " + modelData.start + ": " + modelData.title + (modelData.note ? ". " + modelData.note : ""))
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
        // Tikken naast de kaart sluit; de tik komt niet bij het startscherm eronder
        MouseArea { anchors.fill: parent; onClicked: adminPanel.visible = false }

        // Altijd een witte kaart met donkere tekst (ook bij Glas en volwassen thema), zodat alles leesbaar is
        Rectangle {
            id: adminCard
            anchors.centerIn: parent
            width: Math.min(parent.width - 80 * root.s, 1240 * root.s)
            height: adminRow.implicitHeight + 96 * root.s
            radius: 32 * root.s
            color: "#FFFFFF"
            readonly property color ink: "#1F2A44"
            readonly property color soft: "#5B6478"
            readonly property color green: "#2E8B57"
            MouseArea { anchors.fill: parent }   // tikken op de kaart sluit niet

            RowLayout {
                id: adminRow
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 48 * root.s }
                spacing: 48 * root.s
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 14 * root.s
                    Text { text: "Beheerpagina"; font.pixelSize: 56 * root.s; font.bold: true; color: adminCard.ink }
                    Text {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                        text: "Op deze pc: open de browser en ga naar"
                        font.pixelSize: 30 * root.s; color: adminCard.soft
                    }
                    Text {
                        Layout.fillWidth: true; wrapMode: Text.WrapAnywhere
                        text: "http://localhost:" + (admin.address.split(":").pop() || "8642")
                        font.pixelSize: 40 * root.s; font.bold: true; color: adminCard.ink
                    }
                    Text {
                        Layout.fillWidth: true; Layout.topMargin: 10 * root.s; wrapMode: Text.WordWrap
                        text: "Op een andere pc, tablet of telefoon (zelfde netwerk): scan de QR-code of ga naar"
                        font.pixelSize: 30 * root.s; color: adminCard.soft
                    }
                    Text {
                        Layout.fillWidth: true; wrapMode: Text.WrapAnywhere
                        text: admin.address !== "" ? admin.address : "Geen netwerk gevonden"
                        font.pixelSize: 40 * root.s; font.bold: true; color: adminCard.ink
                    }
                    Text {
                        Layout.topMargin: 10 * root.s
                        text: "Code: " + admin.code
                        font.pixelSize: 48 * root.s; font.bold: true; font.letterSpacing: 6 * root.s; color: adminCard.green
                    }
                    Text {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                        text: "Of log in met de vaste inlognaam en het wachtwoord (in te stellen op de beheerpagina, tabblad Toegang)."
                        font.pixelSize: 26 * root.s; color: adminCard.soft
                    }
                    Row {
                        Layout.topMargin: 16 * root.s
                        spacing: 20 * root.s
                        Rectangle {
                            width: 300 * root.s; height: 90 * root.s; radius: 20 * root.s
                            color: newCodeTap.pressed ? "#DDE3EA" : "#EEF1F5"
                            border.color: "#D9DDE5"; border.width: 2
                            Text { anchors.centerIn: parent; text: "Nieuwe code"; font.pixelSize: 34 * root.s; font.bold: true; color: adminCard.ink }
                            TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds; id: newCodeTap; onTapped: admin.newCode() }
                        }
                        Rectangle {
                            width: 240 * root.s; height: 90 * root.s; radius: 20 * root.s
                            color: closeTap.pressed ? "#245F3E" : adminCard.green
                            Text { anchors.centerIn: parent; text: "Sluiten"; font.pixelSize: 34 * root.s; font.bold: true; color: "#FFFFFF" }
                            TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds; id: closeTap; onTapped: adminPanel.visible = false }
                        }
                    }
                }
                Image {
                    Layout.preferredWidth: 380 * root.s
                    Layout.preferredHeight: 380 * root.s
                    Layout.alignment: Qt.AlignVCenter
                    source: admin.qrImage
                    fillMode: Image.PreserveAspectFit
                    cache: false
                }
            }
        }
    }

    // ---------- Andere pagina's (over het startscherm heen) ----------
    // Vangt alle tikken op die op een pagina niet bij een knop horen, zodat het startscherm
    // eronder niet meereageert (anders sprak de kaart "Nu" bijvoorbeeld bij de spelletjes)
    MouseArea {
        anchors.fill: parent
        visible: root.page !== "home"
    }
    Loader {
        id: pageLoader
        anchors.fill: parent
        active: root.page !== "home"
        sourceComponent: root.page === "stream" ? streamPage : root.page === "presentations" ? presentationPage
                         : root.page === "shortcuts" ? shortcutsPage : root.page === "games" ? gamesPage : root.page === "news" ? newsPage : root.page === "weather" ? weatherPage : root.page === "relax" ? relaxPage : root.page === "talk" ? talkPage : root.page === "travel" ? travelPage : null
        // Zachte overgang: scherm komt iets naar voren en wordt zichtbaar
        onLoaded: pageIn.restart()
        ParallelAnimation {
            id: pageIn
            NumberAnimation { target: pageLoader.item; property: "opacity"; from: 0; to: 1; duration: 320; easing.type: Easing.OutCubic }
            NumberAnimation { target: pageLoader.item; property: "scale"; from: 0.965; to: 1; duration: 380; easing.type: Easing.OutCubic }
        }
    }
    Component { id: streamPage; StreamScreen { s: root.s; home: root; onBack: root.page = "home" } }
    Component { id: gamesPage; GamesScreen { s: root.s; onBack: root.page = "home" } }
    Component { id: talkPage; TalkScreen { s: root.s; onBack: root.page = "home" } }
    Component { id: travelPage; TravelScreen { s: root.s; home: root; onBack: root.page = "home" } }
    Component { id: relaxPage; RelaxScreen { s: root.s; home: root; onBack: root.page = "home" } }
    Component { id: weatherPage; WeatherScreen { s: root.s; onBack: root.page = "home" } }
    Component { id: newsPage; NewsScreen { s: root.s; current: root.newsIndex; onBack: root.page = "home" } }
    Component { id: presentationPage; PresentationScreen { s: root.s; onBack: root.page = "home" } }
    Component { id: shortcutsPage; ShortcutsScreen { s: root.s; onBack: root.page = "home" } }

    // Na 15 minuten terug naar het startscherm (behalve tijdens een presentatie of radio)
    Timer {
        id: idle
        interval: 15 * 60 * 1000
        running: root.page !== "home"
        onTriggered: {
            const busy = (pageLoader.item && pageLoader.item.show !== undefined && pageLoader.item.show !== null)
                         || root.radioRow >= 0 || (pageLoader.item && pageLoader.item.playing === true)
            if (busy) restart(); else root.page = "home"
        }
    }

    Keys.onEscapePressed: {
        if (adminPanel.visible) adminPanel.visible = false
        else if (root.page !== "home") root.page = "home"
        else if (selfTest !== "") Qt.quit()
    }

    // Rooktest ontspanning: alle onderdelen langs, natuurgeluid starten
    Timer {
        id: relaxTest
        interval: 800
        property int n: 0
        repeat: true
        onTriggered: {
            const keys = ["natuur", "gym", "muziek", "zingen", "verhalen", "dieren", "adem"]
            const it = pageLoader.item
            if (!it || n >= keys.length) { stop(); console.info("Relax test done: " + n + " sections"); return }
            it.choose(keys[n++])
        }
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
            } else if (selfTest === "music") {
                const row = radio.rowOfName(radio.nameAt(0))
                console.info("Daypart music: station " + radio.nameAt(0) + " row " + row + " daypart " + agenda.daypart)
                root.playRadio(0)
                root.page = "stream"
                root.page = "home"
                console.info("Music still playing after page change: " + (root.radioRow === 0 ? "yes" : "no"))
            } else if (selfTest === "memory") {
                root.page = "games"
                pageLoader.item.startMemory(6)
            } else if (selfTest === "puzzle") {
                root.page = "games"
                pageLoader.item.startPuzzle(3)
            } else if (selfTest === "news") {
                news.loadSample()
                console.info("News ticker: " + root.tickerItems.length + " items, shown " + (root.tickerItem ? root.tickerItem.title : "-"))
            } else if (selfTest === "extras") {
                agenda.setTestSetting("clockStyle", "analoog")
                agenda.setTestSetting("dayBig", true)
                agenda.setTestSetting("seasonDecor", true)
                agenda.setTestSetting("weatherTile", true)
                agenda.setTestSetting("bigButtons", true)
                console.info("Extras: analog " + root.analog + " dayBig " + root.dayBig + " season " + root.season.name + " big " + root.big
                             + " watchdog " + watchdog.enabled + " beat " + (watchdog.lastBeatAge() < 10000 ? "ok" : "late"))
                console.info("Holidays: " + ["2026-04-05", "2026-04-27", "2025-04-26", "2026-12-05", "2026-05-14", "2026-06-02"].map(d => d + "=" + (agenda.holidayName(d) || "-")).join(" "))
                root.celebrate()
            } else if (selfTest === "reminder") {
                agenda.setTestSetting("remindMinutes", 5)
                root.wakeUntil = Date.now() + 60000
                root.checkReminder(true)
                console.info("Reminder popup: " + (reminderPopup.visible ? "yes" : "no"))
            } else if (selfTest === "freeze") {
                // Alleen handmatig: hoofdthread 15 s vastzetten om de waakhond te testen
                const until = Date.now() + 15000
                while (Date.now() < until) {}
            } else if (selfTest === "board") {
                board.post("Over 10 minuten gaan we wandelen. Trek je jas aan!", "Anja")
                console.info("Board popup: " + (boardPopup.visible ? "yes" : "no"))
                board.markRead(root.boardMsg.id)
                console.info("Board after read: " + (boardPopup.visible ? "still" : "gone"))
            } else if (selfTest === "talk") {
                root.page = "talk"
                pageLoader.item.choose(pageLoader.item.cards[3])
            } else if (selfTest === "travel") {
                transit.loadSample()
                root.page = "travel"
                console.info("Travel departures: " + transit.departures.length)
            } else if (selfTest === "calendar") {
                console.info("Calendar test: " + calendar.selfTest())
            } else if (selfTest === "quiz" || selfTest === "words" || selfTest === "sudoku" || selfTest === "color") {
                root.page = "games"
                pageLoader.item.startExtra(selfTest)
            } else if (selfTest === "relax") {
                root.page = "relax"
                relaxTest.start()
            } else if (selfTest === "weatherpage") {
                root.page = "weather"
            } else if (selfTest === "newspage") {
                news.loadSample()
                root.openNews(news.headlines[0])
            } else if (selfTest === "volume") {
                root.radioRow = 0
                volumePanel.visible = true
                root.setVolume(0.5)
                console.info("Volume test: output " + Math.round(radioPlayer.audioOutput.volume * 100))
            } else if (selfTest === "menuboard") {
                const board = presentations.list.find(x => x.folder === "Menu van de week")
                console.info("Menu board presentation: " + (board ? board.count : 0) + " slides")
                root.page = "presentations"
                pageLoader.item.startNamed("Menu van de week")
            } else if (selfTest === "radiosaver") {
                root.radioRow = 0
                root.radioSaver = true
            } else if (selfTest === "screensaver") {
                root.page = "presentations"
                if (pageLoader.item.startNamed("")) { root.screensaver = true; console.info("Screensaver started") }
            } else if (selfTest === "voice") {
                root.voiceCommand("Wat doen we nu?")
                root.voiceCommand("hoe laat is het")
                root.voiceCommand("zet de radio uit")
                console.info("Voice phrases understood: " + ["wat doen we nu", "wat eten we vandaag", "zet de radio aan", "hoe laat is het", "spelletjes"].filter(x => root.voiceCommand(x).indexOf("begrijp ik niet") < 0).length + "/5")
                root.page = "home"
            } else if (selfTest === "stats") {
                stats.count("tile", "Rooktest")
                stats.count("tile", "Rooktest")
                console.info("Stats test: Rooktest " + stats.countOf("tile", "Rooktest"))
            } else if (selfTest === "backup") {
                admin.backupTest()
            } else if (selfTest === "login") {
                admin.loginTest()
            } else if (selfTest === "glassboard") {
                agenda.setTestSetting("theme", "glas")
                root.page = "presentations"
                pageLoader.item.startNamed("Menu van de week")
            } else if (selfTest === "glasssaver") {
                agenda.setTestSetting("theme", "glas")
                root.radioRow = 0
                root.radioSaver = true
            } else if (selfTest === "glass") {
                agenda.setTestSetting("theme", "glas")
                console.info("Theme: " + (root.glass ? "glas" : "anders") + " ink " + root.ink + " card alpha " + root.glassAlpha.toFixed(2))
            } else if (selfTest === "theme") {
                agenda.setTestSetting("theme", "volwassen")
                console.info("Theme: " + (root.adult ? "volwassen" : "standaard") + " radius " + root.r(28).toFixed(1))
            } else if (selfTest === "background") {
                agenda.setTestSetting("background", { type: "photo", photo: agenda.testPhoto() })
                console.info("Background url set: " + (root.bgPhoto !== "" ? "yes" : "no"))
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

    // ---------- Herinnering: een paar minuten voor een activiteit een melding met geluid ----------
    property var reminded: ({})
    property var reminder: null
    function checkReminder(force) {
        const min = root.cfg.remindMinutes || 0
        if ((min <= 0 && !force) || root.night) return
        const at = new Date(root.now.getTime() + Math.max(1, min) * 60000)
        const hhmm = Qt.formatTime(at, "HH:mm")
        const list = root.items
        for (let i = 0; i < list.length; ++i) {
            const it = list[i]
            const key = Qt.formatDate(root.now, "yyyy-MM-dd") + " " + it.start + " " + it.title
            if ((it.start === hhmm || (force && i === 0)) && !root.reminded[key]) {
                root.reminded[key] = true
                root.reminder = { title: it.title, icon: it.icon || "⏰", imageUrl: it.imageUrl || "", start: it.start, minutes: Math.max(1, min), color: it.color || "" }
                console.info("Reminder: " + it.title + " at " + it.start)
                reminderHide.restart()
                if (root.cfg.remindSound !== false)
                    speaker.announce(root.cfg.speak ? "Over " + Math.max(1, min) + " minuten: " + it.title : "", true)
                return
            }
        }
    }
    Timer { interval: 15000; repeat: true; running: (root.cfg.remindMinutes || 0) > 0; onTriggered: root.checkReminder(false) }
    Timer { id: reminderHide; interval: 2 * 60 * 1000; onTriggered: root.reminder = null }
    Rectangle {
        id: reminderPopup
        anchors.fill: parent
        z: 95
        visible: root.reminder !== null
        color: "#99000000"
        MouseArea { anchors.fill: parent; onClicked: root.reminder = null }
        Rectangle {
            anchors.centerIn: parent
            width: 1100 * root.s; height: 640 * root.s
            radius: 48 * root.s
            color: root.contrast ? "#000000" : root.glass ? "#E61A2036" : "#FFFFFF"
            border.color: root.contrast ? "#FFE14D" : root.glass ? "#66FFFFFF" : "#2E8B57"; border.width: 4 * root.s
            MouseArea { anchors.fill: parent }   // tik op de kaart sluit niet
            GlassBackground { anchors.fill: parent; anchors.margins: 4 * root.s; visible: root.glass; opacity: 0.5; clip: true }
            Column {
                anchors.centerIn: parent
                spacing: 20 * root.s
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 18 * root.s
                    Text { text: "⏰"; font.family: root.emojiFont; font.pixelSize: 54 * root.s }
                    Text {
                        text: "Over " + (root.reminder ? root.reminder.minutes : 0) + " minuten"
                        font.pixelSize: 54 * root.s; font.bold: true
                        color: root.contrast ? "#FFE14D" : root.glass ? "#8BE9C0" : "#2E8B57"
                    }
                }
                Picto {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 180 * root.s; height: 180 * root.s
                    it: root.reminder
                    size: 150 * root.s
                    SequentialAnimation on scale {
                        running: reminderPopup.visible; loops: Animation.Infinite
                        NumberAnimation { to: 1.08; duration: 700; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: 700; easing.type: Easing.InOutSine }
                    }
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 1000 * root.s; horizontalAlignment: Text.AlignHCenter
                    text: root.reminder ? root.reminder.title : ""
                    font.pixelSize: 76 * root.s; font.bold: true
                    color: root.glass || root.contrast ? "#FFFFFF" : "#1F2A44"
                    wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.reminder ? "Om " + root.reminder.start + " uur" : ""
                    font.pixelSize: 44 * root.s; color: root.glass || root.contrast ? "#C3CAE0" : "#5B6478"
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 320 * root.s * root.big; height: 100 * root.s * root.big; radius: height / 2
                    color: okTap.pressed ? "#245F3E" : "#2E8B57"
                    Text { anchors.centerIn: parent; text: "Oké"; font.pixelSize: 44 * root.s; font.bold: true; color: "#FFFFFF" }
                    TapHandler { id: okTap; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: root.reminder = null }
                }
            }
        }
    }

    // ---------- Berichtenbord: bericht van de begeleiding (beheer: Communicatie > Berichtenbord) ----------
    readonly property var boardMsg: board.current
    Connections {
        target: board
        function onArrived() {
            root.screensaver = false
            if (root.page === "presentations" && pageLoader.item && pageLoader.item.show) root.page = "home"
            if (!root.night) speaker.announce(root.tapSpeak && root.boardMsg.text ? "Bericht" + (root.boardMsg.from ? " van " + root.boardMsg.from : "") + ": " + root.boardMsg.text : "", true)
            console.info("Board message shown")
        }
    }
    Rectangle {
        id: boardPopup
        anchors.fill: parent
        z: 96
        visible: !!root.boardMsg.id
        color: "#99000000"
        MouseArea { anchors.fill: parent }   // tik ernaast doet niets: eerst "Gelezen"
        Rectangle {
            anchors.centerIn: parent
            width: 1300 * root.s; height: 700 * root.s
            radius: 48 * root.s
            color: root.contrast ? "#000000" : root.glass ? "#E61A2036" : "#FFFFFF"
            border.color: root.contrast ? "#FFE14D" : root.glass ? "#66FFFFFF" : "#5DA9E9"; border.width: 4 * root.s
            GlassBackground { anchors.fill: parent; anchors.margins: 4 * root.s; visible: root.glass; opacity: 0.5; clip: true }
            ColumnLayout {
                anchors { fill: parent; margins: 56 * root.s }
                spacing: 20 * root.s
                Row {
                    spacing: 20 * root.s
                    Text { text: "💬"; font.family: root.emojiFont; font.pixelSize: 64 * root.s }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Bericht" + (root.boardMsg.from ? " van " + root.boardMsg.from : "")
                              + (root.boardMsg.time ? "  ·  " + root.boardMsg.time.substring(11, 16) + " uur" : "")
                        font.pixelSize: 48 * root.s; font.bold: true
                        color: root.contrast ? "#FFE14D" : root.glass ? "#8BE9C0" : "#2E8B57"
                    }
                }
                Text {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    text: root.boardMsg.text || ""
                    font.pixelSize: 72 * root.s; font.bold: true
                    color: root.glass || root.contrast ? "#FFFFFF" : "#1F2A44"
                    wrapMode: Text.WordWrap; elide: Text.ElideRight
                    fontSizeMode: boardPopup.visible ? Text.Fit : Text.FixedSize; minimumPixelSize: 36 * root.s
                    verticalAlignment: Text.AlignVCenter
                }
                Row {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 30 * root.s
                    Rectangle {
                        width: 380 * root.s * root.big; height: 110 * root.s * root.big; radius: height / 2
                        color: readTap.pressed ? "#245F3E" : "#2E8B57"
                        Text { anchors.centerIn: parent; text: "✓  Gelezen"; font.pixelSize: 46 * root.s; font.bold: true; color: "#FFFFFF" }
                        TapHandler { id: readTap; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: board.markRead(root.boardMsg.id) }
                    }
                    Rectangle {
                        width: 380 * root.s * root.big; height: 110 * root.s * root.big; radius: height / 2
                        color: sayTap.pressed ? "#40FFFFFF" : root.glass ? "#26FFFFFF" : "#EEF1F5"
                        border.color: root.glass ? "#55FFFFFF" : "#5DA9E9"; border.width: 2
                        Text { anchors.centerIn: parent; text: "🔊  Voorlezen"; font.pixelSize: 46 * root.s; font.bold: true; color: root.glass || root.contrast ? "#FFFFFF" : "#1F2A44" }
                        TapHandler { id: sayTap; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: speaker.say(root.boardMsg.text || "") }
                    }
                }
            }
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
            const busy = (item && item.show !== undefined && item.show !== null) || root.radioRow >= 0
            if (min > 0 && !root.screensaver && !root.night && !busy && !adminPanel.visible
                    && activity.idleSeconds() >= min * 60 && presentations.list.length > 0) {
                root.page = "presentations"
                if (pageLoader.item) pageLoader.item.kenBurns = root.cfg.saverFade !== false
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

    // ---------- Radio: speelt door op alle schermen ----------
    property int radioRow: -1
    readonly property bool radioPlaying: radioPlayer.playbackState === MediaPlayer.PlayingState
    readonly property string radioName: radioRow >= 0 ? radio.nameAt(radioRow) : ""
    MediaPlayer {
        id: radioPlayer
        audioOutput: AudioOutput { volume: root.radioVolume }
        onErrorOccurred: (error, text) => console.warn("Radio error: " + text)
        onPlaybackStateChanged: console.info("Radio state " + playbackState)
    }
    function playRadio(row) {
        const streams = radio.streamsAt(row)
        if (!streams || streams.length === 0) return false
        radioRow = row
        radioPlayer.source = streams[0]
        radioPlayer.play()
        console.info("Radio playing: " + radio.nameAt(row))
        stats.count("radio", radio.nameAt(row))
        return true
    }
    function stopRadio() { radioPlayer.stop(); radioRow = -1; radioSaver = false; volumePanel.visible = false }
    // Radiovolume (0 tot 1, in stappen van 10%), wordt onthouden
    property real radioVolume: Math.max(0, Math.min(1, Number(agenda.uiValue("radioVolume", 0.8))))
    function setVolume(v) {
        radioVolume = Math.max(0, Math.min(1, Math.round(v * 10) / 10))
        agenda.setUiValue("radioVolume", radioVolume)
        volumeHide.restart()
        console.info("Radio volume " + Math.round(radioVolume * 100))
    }
    Timer { id: volumeHide; interval: 6000; onTriggered: volumePanel.visible = false }
    onPageChanged: volumePanel.visible = false
    Rectangle {
        id: volumePanel
        z: 85
        visible: false
        anchors { right: parent.right; top: header.bottom; rightMargin: 48 * root.s; topMargin: 12 * root.s }
        width: volRow.implicitWidth + 48 * root.s; height: 140 * root.s
        radius: 70 * root.s
        color: root.contrast ? "#000000" : root.glass ? "#E61A2036" : "#FFFFFF"
        border.color: root.contrast ? "#FFE14D" : root.glass ? "#55FFFFFF" : "#2E8B57"; border.width: 3 * root.s
        MouseArea { anchors.fill: parent; onPressed: volumeHide.restart() }   // tik gaat niet door
        Row {
            id: volRow
            anchors.centerIn: parent
            spacing: 18 * root.s
            VolButton { anchors.verticalCenter: parent.verticalCenter; label: "−"; onHit: root.setVolume(root.radioVolume - 0.1) }
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8 * root.s
                Repeater {
                    model: 10
                    delegate: Rectangle {
                        required property int index
                        anchors.bottom: parent.bottom
                        width: 26 * root.s; height: (30 + index * 6) * root.s; radius: 6 * root.s
                        color: index < Math.round(root.radioVolume * 10) ? "#2E8B57" : (root.glass ? "#40FFFFFF" : "#DDE3EA")
                        TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: root.setVolume((index + 1) / 10) }
                    }
                }
            }
            VolButton { anchors.verticalCenter: parent.verticalCenter; label: "+"; onHit: root.setVolume(root.radioVolume + 0.1) }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: 110 * root.s
                horizontalAlignment: Text.AlignRight
                text: Math.round(root.radioVolume * 100) + "%"
                font.pixelSize: 40 * root.s; font.bold: true; color: root.glass || root.contrast ? "#FFFFFF" : "#1F2A44"
            }
        }
    }

    // ---------- Muziek per dagdeel ----------
    readonly property var music: cfg.music || {}
    readonly property string daypartStation: music[agenda.daypart] || ""
    function playDaypart() {
        const row = daypartStation !== "" ? radio.rowOfName(daypartStation) : -1
        if (row >= 0 && radio.isStation(row)) return playRadio(row)
        console.info("Daypart music: no station for " + agenda.daypart)
        return false
    }
    readonly property string daypartNow: agenda.daypart
    onDaypartNowChanged: {
        if (!music.auto) return
        console.info("Daypart music: " + daypartNow + " " + daypartStation)
        if (daypartNow === "nacht" || daypartStation === "") stopRadio()
        else if (radioName !== daypartStation) playDaypart()
    }

    // ---------- Spraakbediening ----------
    property string voiceText: ""
    property bool voiceAsked: false      // meldingen van de spraakherkenning pas tonen na een tik op de microfoon
    function voiceReply(text, speak) {
        voiceText = text
        voiceHide.restart()
        if (speak && text !== "") speaker.say(text)
    }
    Timer { id: voiceHide; interval: 7000; onTriggered: root.voiceText = "" }
    Connections {
        target: listener
        function onHeard(text) { root.voiceCommand(text) }
        function onNothingHeard() { root.voiceReply("Ik heb niets verstaan. Tik nog eens op de microfoon.", false) }
        function onStateChanged() { if (root.voiceAsked && listener.state === "error" && listener.message !== "") root.voiceReply(listener.message, false) }
    }
    // Zin -> opdracht; geeft het antwoord terug (en voert het uit)
    function voiceCommand(text) {
        const t = String(text).toLowerCase().replace(/[?!.,]/g, "").trim()
        const has = (...words) => words.some(w => t.indexOf(w) >= 0)
        let key = "", answer = ""
        if (has("radio aan", "muziek aan")) {
            key = "radio-aan"
            if (root.radioRow >= 0) answer = "De radio staat al aan: " + root.radioName
            else if (root.playDaypart() || root.playRadio(0)) answer = "Ik zet " + root.radioName + " aan."
            else answer = "Er is geen radiozender ingesteld."
        } else if (has("radio uit", "muziek uit", "stop")) {
            key = "radio-uit"; root.stopRadio(); answer = "De muziek is uit."
        } else if (has("volgende zender")) {
            key = "volgende-zender"
            let row = root.radioRow + 1
            while (row < radio.count && !radio.isStation(row)) row++
            if (row >= radio.count) row = 0
            answer = root.playRadio(row) ? "Nu speelt " + root.radioName + "." : "Dat lukt niet."
        } else if (has("straks", "volgende")) {
            key = "straks"
            answer = root.next ? "Straks, om " + root.next.start + ": " + root.next.title + "." : "Vandaag staat er niets meer op de planning."
        } else if (has("doen we nu", "gaan we doen", "is er nu")) {
            key = "nu"
            answer = root.current ? "Nu: " + root.current.title + (root.current.note ? ". " + root.current.note : "") + "."
                                  : (root.next ? "Nu even niets. Straks, om " + root.next.start + ": " + root.next.title + "." : "Nu staat er niets op de planning.")
        } else if (has("eten")) {
            key = "eten"
            answer = agenda.todayMenu.dish ? "Vandaag eten we " + agenda.todayMenu.dish + "." : "Er staat vandaag geen eten in het weekmenu."
        } else if (has("hoe laat")) {
            key = "tijd"
            const n = new Date()
            answer = "Het is " + n.getHours() + " uur " + (n.getMinutes() ? n.getMinutes() : "") + "."
        } else if (has("welke dag", "voor dag", "datum")) {
            key = "dag"
            answer = "Het is " + new Date().toLocaleDateString(Qt.locale("nl_NL"), "dddd d MMMM") + "."
        } else if (has("spelletje", "memory", "puzzel")) {
            key = "spelletjes"; root.page = "games"; answer = "Veel plezier met de spelletjes!"
        } else if (has("presentaties")) {
            key = "presentaties"; root.page = "presentations"; answer = "Hier zijn de presentaties."
        } else if (has("kijk en luister")) {
            key = "kijk-en-luister"; root.page = "stream"; answer = "Kijk en luister."
        } else if (has("terug", "begin", "startscherm")) {
            key = "startscherm"; root.page = "home"; answer = "Terug naar het begin."
        } else {
            answer = "Dat begrijp ik niet. Zeg bijvoorbeeld: wat doen we nu?"
        }
        console.info("Voice command: " + (key || "unknown") + " (" + t + ")")
        if (key !== "") stats.count("voice", key)
        root.voiceReply(answer, true)
        return answer
    }
    // Ballon met wat de kiosk verstond / antwoordt
    Rectangle {
        id: voiceBubble
        z: 90
        visible: root.voiceText !== ""
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 60 * root.s }
        width: Math.min(parent.width - 200 * root.s, voiceRow.implicitWidth + 100 * root.s)
        height: voiceRow.implicitHeight + 60 * root.s
        radius: root.r(36)
        color: root.contrast ? "#000000" : root.glass ? "#D91A2036" : "#1F2A44"
        border.color: root.contrast ? "#FFE14D" : root.glass ? "#55FFFFFF" : "transparent"; border.width: root.lineWidth
        Row {
            id: voiceRow
            anchors.centerIn: parent
            spacing: 20 * root.s
            Text { anchors.verticalCenter: parent.verticalCenter; text: "🎤"; font.family: root.emojiFont; font.pixelSize: 44 * root.f }
            Text {
                id: voiceLabel
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, root.width - 400 * root.s)
                text: root.voiceText
                font.pixelSize: 44 * root.f; font.bold: true; color: "#FFFFFF"
                wrapMode: Text.WordWrap
            }
        }
        MouseArea { anchors.fill: parent; onClicked: root.voiceText = "" }
    }

    // ---------- Radio-screensaver: de zender zweeft over het scherm ----------
    property bool radioSaver: false
    onRadioSaverChanged: console.info(radioSaver ? "Radio screensaver started" : "Radio screensaver stopped")
    Timer {
        // Radio aan (vanaf het startscherm of de Radio-pagina): na 1 minuut (instelbaar) zweeft de zender over het scherm
        interval: 5000; running: root.radioRow >= 0 && !root.radioSaver && !root.night && (root.cfg.radioSaverMinutes ?? 1) > 0; repeat: true
        onTriggered: {
            const min = root.cfg.radioSaverMinutes ?? 1
            if (activity.idleSeconds() >= min * 60) { root.screensaver = false; root.radioSaver = true }
        }
    }
    Connections {
        target: activity
        function onTouched() { root.radioSaver = false }
    }
    Rectangle {
        id: saverLayer
        anchors.fill: parent
        visible: root.radioSaver && root.radioRow >= 0
        z: 95
        gradient: Gradient {
            GradientStop { position: 0; color: "#0E1630" }
            GradientStop { position: 1; color: "#05070F" }
        }
        GlassBackground { anchors.fill: parent; visible: root.glass; animate: saverLayer.visible }
        // Tik ergens: screensaver weg, radio speelt door
        MouseArea { anchors.fill: parent; onPressed: root.radioSaver = false }

        property real vx: 2.2 * root.s
        property real vy: 1.6 * root.s
        property color accent: root.radioRow >= 0 ? String(radio.accentAt(root.radioRow) || "#F2B33D") : "#F2B33D"
        readonly property var accents: ["#F2B33D", "#E85D75", "#4FC3F7", "#8CC084", "#C3B1E1", "#FF8A3D"]
        property int bounce: 0

        Rectangle {
            id: floater
            width: 720 * root.s; height: 300 * root.s
            radius: root.r(48)
            color: root.glass ? Qt.rgba(1, 1, 1, root.glassAlpha) : "#1B2547"
            border.color: root.glass ? "#66FFFFFF" : saverLayer.bounce > 0 ? saverLayer.accents[saverLayer.bounce % saverLayer.accents.length] : saverLayer.accent
            border.width: (root.glass ? 2 : 8) * root.s
            x: (saverLayer.width - width) / 2; y: (saverLayer.height - height) / 2
            RowLayout {
                anchors { fill: parent; margins: 36 * root.s }
                spacing: 32 * root.s
                Rectangle {
                    Layout.preferredWidth: 200 * root.s; Layout.preferredHeight: 200 * root.s
                    radius: root.r(36)
                    color: saverLayer.accent
                    Image {
                        id: saverLogo
                        anchors { fill: parent; margins: 24 * root.s }
                        fillMode: Image.PreserveAspectFit
                        source: root.radioRow >= 0 && !String(radio.iconAt(root.radioRow)).startsWith("glyph:") ? radio.iconAt(root.radioRow) : ""
                        sourceSize.width: 256
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: saverLogo.status !== Image.Ready
                        text: "📻"
                        font.family: Qt.platform.os === "windows" ? "Segoe UI Emoji" : "Noto Color Emoji"
                        font.pixelSize: 110 * root.s
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8 * root.s
                    Text {
                        Layout.fillWidth: true
                        text: root.radioRow >= 0 ? radio.nameAt(root.radioRow) : ""
                        font.pixelSize: 64 * root.s; font.bold: true; color: "#FFFFFF"
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "♪  Nu op de radio"
                        font.pixelSize: 38 * root.s; color: "#AEB8D6"
                    }
                }
            }
        }
        Timer {
            interval: 33; repeat: true; running: saverLayer.visible
            onTriggered: {
                let nx = floater.x + saverLayer.vx, ny = floater.y + saverLayer.vy
                if (nx <= 0 || nx + floater.width >= saverLayer.width) { saverLayer.vx = -saverLayer.vx; saverLayer.bounce++ }
                if (ny <= 0 || ny + floater.height >= saverLayer.height) { saverLayer.vy = -saverLayer.vy; saverLayer.bounce++ }
                floater.x = Math.max(0, Math.min(nx, saverLayer.width - floater.width))
                floater.y = Math.max(0, Math.min(ny, saverLayer.height - floater.height))
            }
        }
        Text {
            anchors { right: parent.right; bottom: parent.bottom; margins: 40 * root.s }
            text: "Tik op het scherm om terug te gaan"
            font.pixelSize: 30 * root.s; color: "#5B6688"
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
        if (night) { root.screensaver = false; root.page = "home"; if (root.music.auto) root.stopRadio() }
    }
    Rectangle {
        anchors.fill: parent
        visible: root.night
        color: "#000000"
        opacity: root.cfg.nightMode === "black" ? 1 : (root.cfg.nightDim || 88) / 100
        z: 100
        Text {
            anchors.centerIn: parent
            visible: root.cfg.nightMode !== "black"
            text: "🌙  " + Qt.formatTime(root.now, "HH:mm")
            font.family: root.emojiFont
            font.pixelSize: 140 * root.s; color: "#8090B0"
        }
        // Tik: 2 minuten wakker. De tik zelf gaat niet door naar het startscherm.
        MouseArea { anchors.fill: parent; onPressed: root.wakeUntil = Date.now() + 2 * 60 * 1000 }
    }
}
