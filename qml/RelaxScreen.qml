// Ontspanning: ademhalen, natuurgeluiden, stoelgym, eigen muziek, meezingen, verhalen en dieren live.
// Altijd in de glasstijl. Links de onderdelen als grote knoppen, rechts het gekozen onderdeel.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts
import QtMultimedia

Rectangle {
    id: page
    color: "#0A0F1E"
    required property real s
    required property var home
    property string section: "adem"
    signal back()

    readonly property color glassCard: "#1FFFFFFF"
    readonly property color glassLine: "#40FFFFFF"
    readonly property color mint: "#8BE9C0"
    readonly property string emojiFont: Qt.platform.os === "windows" ? "Segoe UI Emoji" : "Noto Color Emoji"

    readonly property var sections: [
        { key: "adem", icon: "🌬️", title: "Ademhalen" },
        { key: "natuur", icon: "🌧️", title: "Natuurgeluiden" },
        { key: "gym", icon: "🪑", title: "Stoelgym" },
        { key: "muziek", icon: "💿", title: "Mijn muziek" },
        { key: "zingen", icon: "🎤", title: "Meezingen" },
        { key: "verhalen", icon: "📖", title: "Verhalen" },
        { key: "dieren", icon: "🦁", title: "Dieren live" }
    ]
    function links(group) { return agenda.relaxLinks.filter(l => l.group === group) }
    function openLink(url, title) {
        page.home.stopRadio()
        nature.stop()
        appModel.launchWeb(url)
        homeBridge.externalLaunched()
        stats.count("relax", title)
        console.info("Relax link opened: " + title)
    }
    function choose(k) {
        section = k
        idle.restart()
        stats.count("relax", k)
        console.info("Relax section: " + k)
    }

    // Na 10 minuten niets aanraken terug (niet zolang er een geluid of oefening loopt)
    Timer { id: idle; interval: 10 * 60 * 1000; running: true; onTriggered: if (!nature.playing && !gym.running && !breath.running) page.back() }

    component Glass: Rectangle {
        radius: 32 * page.s
        color: page.glassCard
        border.color: page.glassLine; border.width: 2
    }
    component Pill: Rectangle {
        id: pill
        property string label
        property bool on: false
        signal hit()
        implicitHeight: 96 * page.s
        implicitWidth: pillText.implicitWidth + 80 * page.s
        radius: height / 2
        color: pillTap.pressed ? "#55FFFFFF" : on ? "#662E8B57" : "#26FFFFFF"
        border.color: on ? page.mint : page.glassLine; border.width: on ? 3 : 2
        Text { id: pillText; anchors.centerIn: parent; text: pill.label; font.pixelSize: 38 * page.s; font.bold: true; color: "#FFFFFF" }
        TapHandler { id: pillTap; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: pill.hit() }
    }
    component LinkGrid: GridView {
        id: lg
        property var items: []
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        cellWidth: width / 2
        cellHeight: 190 * page.s
        model: items
        delegate: Item {
            required property var modelData
            width: lg.cellWidth; height: lg.cellHeight
            Rectangle {
                anchors { fill: parent; margins: 12 * page.s }
                radius: 28 * page.s
                color: linkTap.pressed ? "#40FFFFFF" : page.glassCard
                border.color: page.glassLine; border.width: 2
                RowLayout {
                    anchors { fill: parent; leftMargin: 28 * page.s; rightMargin: 28 * page.s }
                    spacing: 24 * page.s
                    Text { text: modelData.icon || "▶️"; font.family: page.emojiFont; font.pixelSize: 72 * page.s }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.title
                        font.pixelSize: 38 * page.s; font.bold: true; color: "#FFFFFF"
                        wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                    }
                }
                TapHandler { id: linkTap; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: page.openLink(modelData.url, modelData.title) }
            }
        }
        Text {
            anchors.centerIn: parent
            visible: lg.count === 0
            width: parent.width * 0.8
            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            text: "Hier staat nog niets. Voeg links toe op de beheerpagina (Media > Ontspanning)."
            font.pixelSize: 38 * page.s; color: "#C3CAE0"
        }
    }

    GlassBackground { anchors.fill: parent }

    // ---------- Kop ----------
    RowLayout {
        id: top
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 40 * page.s }
        height: 110 * page.s
        spacing: 24 * page.s
        Pill { label: "‹  Terug"; onHit: { nature.stop(); page.back() } }
        Text {
            Layout.fillWidth: true
            text: "Ontspanning"
            font.pixelSize: 72 * page.s; font.bold: true; color: "#FFFFFF"
        }
        // Speelt er een natuurgeluid: altijd zichtbaar, ook in een ander onderdeel
        Pill {
            visible: nature.playing
            label: "⏹  " + nature.title + " stoppen"
            onHit: nature.stop()
        }
    }

    // ---------- Onderdelen ----------
    Column {
        id: menu
        anchors { left: parent.left; top: top.bottom; bottom: parent.bottom; margins: 40 * page.s; topMargin: 24 * page.s }
        width: 420 * page.s
        spacing: 14 * page.s
        Repeater {
            model: page.sections
            delegate: Rectangle {
                required property var modelData
                readonly property bool on: page.section === modelData.key
                width: menu.width
                height: (menu.height - 6 * menu.spacing) / 7
                radius: height / 2
                color: on ? "#662E8B57" : secTap.pressed ? "#40FFFFFF" : page.glassCard
                border.color: on ? page.mint : page.glassLine; border.width: on ? 3 : 2
                RowLayout {
                    anchors { fill: parent; leftMargin: 30 * page.s; rightMargin: 24 * page.s }
                    spacing: 20 * page.s
                    Text { text: modelData.icon; font.family: page.emojiFont; font.pixelSize: 50 * page.s }
                    Text { Layout.fillWidth: true; text: modelData.title; font.pixelSize: 38 * page.s; font.bold: true; color: "#FFFFFF"; elide: Text.ElideRight }
                }
                TapHandler { id: secTap; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: page.choose(modelData.key) }
            }
        }
    }

    Item {
        id: content
        anchors { left: menu.right; right: parent.right; top: top.bottom; bottom: parent.bottom; margins: 40 * page.s; topMargin: 24 * page.s }

        // ---------- Ademhalen ----------
        Glass {
            id: breath
            anchors.fill: parent
            visible: page.section === "adem"
            property bool running: false
            property string phase: "Tik op start"
            property int rounds: 0
            property real size: 0.45
            onVisibleChanged: if (!visible) stop()
            function stop() { running = false; cycle.stop(); phase = "Tik op start"; size = 0.45 }
            function start() { running = true; rounds = 0; cycle.restart(); console.info("Breathing started") }
            SequentialAnimation {
                id: cycle
                loops: Animation.Infinite
                ScriptAction { script: breath.phase = "Adem in" }
                NumberAnimation { target: breath; property: "size"; to: 1.0; duration: 4000; easing.type: Easing.InOutSine }
                ScriptAction { script: breath.phase = "Vasthouden" }
                PauseAnimation { duration: 2000 }
                ScriptAction { script: breath.phase = "Adem uit" }
                NumberAnimation { target: breath; property: "size"; to: 0.45; duration: 6000; easing.type: Easing.InOutSine }
                ScriptAction { script: { breath.rounds++; if (breath.rounds >= 10) breath.stop() } }
            }
            Item {
                id: orbBox
                anchors { left: parent.left; right: parent.right; top: parent.top; bottom: breathControls.top; margins: 30 * page.s }
                readonly property real d: Math.min(width, height)
                Rectangle {
                    anchors.centerIn: parent
                    width: orbBox.d * breath.size; height: width
                    radius: width / 2
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#AA8BE9C0" }
                        GradientStop { position: 1; color: "#AA5DA9E9" }
                    }
                    border.color: "#99FFFFFF"; border.width: 3
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 1.18; height: width; radius: width / 2
                        color: "transparent"; border.color: "#33FFFFFF"; border.width: 2
                    }
                }
                Column {
                    anchors.centerIn: parent
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: breath.phase; font.pixelSize: 76 * page.s; font.bold: true; color: "#FFFFFF"; style: Text.Outline; styleColor: "#33000000" }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: breath.running
                        text: "Ronde " + (breath.rounds + 1) + " van 10"
                        font.pixelSize: 34 * page.s; color: "#E6EAF5"
                    }
                }
            }
            RowLayout {
                id: breathControls
                anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 36 * page.s }
                spacing: 24 * page.s
                Pill { label: breath.running ? "⏸  Stoppen" : "▶  Start"; onHit: breath.running ? breath.stop() : breath.start() }
                Pill {
                    label: nature.playing && nature.key === "zee" ? "🌊  Zee uit" : "🌊  Met zee-geluid"
                    on: nature.playing && nature.key === "zee"
                    onHit: nature.playing && nature.key === "zee" ? nature.stop() : nature.play("zee", "Zee")
                }
            }
        }

        // ---------- Natuurgeluiden ----------
        Item {
            anchors.fill: parent
            visible: page.section === "natuur"
            GridLayout {
                id: soundGrid
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: parent.height - sleepRow.height - 30 * page.s
                columns: 2
                rowSpacing: 24 * page.s; columnSpacing: 24 * page.s
                Repeater {
                    model: [
                        { key: "regen", icon: "🌧️", title: "Regen" },
                        { key: "zee", icon: "🌊", title: "Zee" },
                        { key: "bos", icon: "🌳", title: "Bos met vogels" },
                        { key: "beek", icon: "💧", title: "Kabbelend beekje" }
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        readonly property bool on: nature.playing && nature.key === modelData.key
                        Layout.fillWidth: true; Layout.fillHeight: true
                        radius: 36 * page.s
                        color: on ? "#662E8B57" : sndTap.pressed ? "#40FFFFFF" : page.glassCard
                        border.color: on ? page.mint : page.glassLine; border.width: on ? 4 : 2
                        Column {
                            anchors.centerIn: parent
                            spacing: 10 * page.s
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.icon; font.family: page.emojiFont; font.pixelSize: 110 * page.s
                                SequentialAnimation on scale {
                                    running: on; loops: Animation.Infinite
                                    NumberAnimation { to: 1.08; duration: 1400; easing.type: Easing.InOutSine }
                                    NumberAnimation { to: 1.0; duration: 1400; easing.type: Easing.InOutSine }
                                }
                            }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.title; font.pixelSize: 46 * page.s; font.bold: true; color: "#FFFFFF" }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: on ? "speelt · tik om te stoppen" : "tik om te luisteren"; font.pixelSize: 30 * page.s; color: "#C3CAE0" }
                        }
                        TapHandler { id: sndTap; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: on ? nature.stop() : nature.play(modelData.key, modelData.title) }
                    }
                }
            }
            RowLayout {
                id: sleepRow
                anchors { left: parent.left; bottom: parent.bottom }
                spacing: 18 * page.s
                Text { text: "Stopt vanzelf na:"; font.pixelSize: 36 * page.s; color: "#E6EAF5" }
                Repeater {
                    model: [15, 30, 60]
                    delegate: Pill {
                        required property int modelData
                        label: modelData + " min"
                        on: nature.minutes === modelData
                        onHit: { nature.minutes = modelData; if (nature.playing) nature.sleep.restart() }
                    }
                }
            }
        }

        // ---------- Stoelgym ----------
        Glass {
            id: gym
            anchors.fill: parent
            visible: page.section === "gym"
            property bool running: false
            property int step: 0
            property int remaining: 0
            property bool speak: true
            readonly property var steps: [
                { icon: "🙆", title: "Schouders ophalen", text: "Trek je schouders op naar je oren. Laat ze langzaam weer zakken.", sec: 30 },
                { icon: "🔄", title: "Hoofd draaien", text: "Draai je hoofd rustig naar links. En dan naar rechts.", sec: 30 },
                { icon: "🍎", title: "Appels plukken", text: "Strek je arm omhoog alsof je een appel plukt. Om en om, links en rechts.", sec: 40 },
                { icon: "✋", title: "Handen open en dicht", text: "Maak een vuist. Doe je hand weer wijd open.", sec: 30 },
                { icon: "🌀", title: "Polsen draaien", text: "Draai rondjes met je polsen. Eerst de ene kant op, dan de andere.", sec: 30 },
                { icon: "🦶", title: "Tenen tikken", text: "Tik met je tenen op de grond. Om en om, links en rechts.", sec: 40 },
                { icon: "🦵", title: "Knieën optillen", text: "Til je knie een stukje op. Zet hem neer. Nu de andere knie.", sec: 40 },
                { icon: "🚲", title: "Fietsen", text: "Doe alsof je fietst met je voeten. Rustig aan.", sec: 30 },
                { icon: "🤗", title: "Jezelf een knuffel", text: "Sla je armen om je heen. Goed gedaan!", sec: 20 },
                { icon: "🌬️", title: "Rustig ademen", text: "Adem diep in door je neus. En rustig weer uit.", sec: 30 }
            ]
            readonly property var cur: steps[Math.min(step, steps.length - 1)]
            onVisibleChanged: if (!visible) stop()
            function say() { if (speak) speaker.say(cur.title + ". " + cur.text) }
            function start() { step = 0; remaining = steps[0].sec; running = true; say(); console.info("Chair gym started: " + steps.length + " exercises") }
            function stop() { running = false; tick.stop() }
            function go(d) { step = Math.max(0, Math.min(steps.length - 1, step + d)); remaining = cur.sec; say() }
            Timer {
                id: tick
                interval: 1000; repeat: true; running: gym.running
                onTriggered: {
                    gym.remaining--
                    if (gym.remaining <= 0) {
                        if (gym.step >= gym.steps.length - 1) { gym.stop(); gym.step = gym.steps.length; if (gym.speak) speaker.say("Klaar! Goed gedaan.") }
                        else gym.go(1)
                    }
                }
            }
            ColumnLayout {
                anchors { fill: parent; margins: 40 * page.s }
                spacing: 20 * page.s
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: gym.running ? "Oefening " + (gym.step + 1) + " van " + gym.steps.length : gym.step >= gym.steps.length ? "Klaar! Goed gedaan 👏" : "Bewegen op je stoel: 10 rustige oefeningen"
                    font.pixelSize: 40 * page.s; font.bold: true; color: page.mint
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: gym.cur.icon; font.family: page.emojiFont; font.pixelSize: 200 * page.s
                    SequentialAnimation on rotation {
                        running: gym.running; loops: Animation.Infinite
                        NumberAnimation { to: 8; duration: 900; easing.type: Easing.InOutSine }
                        NumberAnimation { to: -8; duration: 900; easing.type: Easing.InOutSine }
                    }
                }
                Text { Layout.alignment: Qt.AlignHCenter; text: gym.cur.title; font.pixelSize: 72 * page.s; font.bold: true; color: "#FFFFFF" }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                    text: gym.cur.text; font.pixelSize: 44 * page.s; color: "#E6EAF5"
                }
                // Hoeveel tijd nog
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22 * page.s
                    radius: height / 2
                    color: "#26FFFFFF"
                    visible: gym.running
                    Rectangle {
                        width: parent.width * (gym.remaining / Math.max(1, gym.cur.sec)); height: parent.height; radius: height / 2
                        color: page.mint
                        Behavior on width { NumberAnimation { duration: 900 } }
                    }
                }
                Item { Layout.fillHeight: true }
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 20 * page.s
                    Pill { visible: gym.running; label: "‹  Vorige"; onHit: gym.go(-1) }
                    Pill { label: gym.running ? "⏸  Stoppen" : "▶  Beginnen"; onHit: gym.running ? gym.stop() : gym.start() }
                    Pill { visible: gym.running; label: "Volgende  ›"; onHit: gym.go(1) }
                    Pill { label: gym.speak ? "🔊 Voorlezen aan" : "🔇 Voorlezen uit"; on: gym.speak; onHit: gym.speak = !gym.speak }
                    Pill { visible: page.links("gym").length > 0 && !gym.running; label: "🎬 Video"; onHit: page.openLink(page.links("gym")[0].url, page.links("gym")[0].title) }
                }
            }
        }

        // ---------- Mijn muziek: grote albumhoezen ----------
        GridView {
            id: albums
            anchors.fill: parent
            visible: page.section === "muziek"
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            cellWidth: width / 3
            cellHeight: cellWidth + 110 * page.s
            model: agenda.favorites
            delegate: Item {
                required property var modelData
                width: albums.cellWidth; height: albums.cellHeight
                Rectangle {
                    anchors { fill: parent; margins: 14 * page.s }
                    radius: 28 * page.s
                    color: albTap.pressed ? "#40FFFFFF" : page.glassCard
                    border.color: page.glassLine; border.width: 2
                    clip: true
                    Rectangle {
                        id: cover
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 18 * page.s }
                        height: width
                        radius: 18 * page.s
                        color: "#33FFFFFF"
                        clip: true
                        Image {
                            anchors.fill: parent
                            source: modelData.coverUrl || ""
                            visible: !!modelData.coverUrl
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            sourceSize.width: 600
                        }
                        Text { anchors.centerIn: parent; visible: !modelData.coverUrl; text: "💿"; font.family: page.emojiFont; font.pixelSize: 140 * page.s }
                    }
                    Column {
                        anchors { left: parent.left; right: parent.right; top: cover.bottom; margins: 18 * page.s; topMargin: 10 * page.s }
                        Text { width: parent.width; text: modelData.title; font.pixelSize: 34 * page.s; font.bold: true; color: "#FFFFFF"; elide: Text.ElideRight }
                        Text { width: parent.width; text: modelData.artist || ""; font.pixelSize: 28 * page.s; color: "#C3CAE0"; elide: Text.ElideRight }
                    }
                    TapHandler {
                        id: albTap
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onTapped: page.openLink(modelData.url || ("https://www.youtube.com/results?search_query=" + encodeURIComponent((modelData.artist || "") + " " + modelData.title)), modelData.title)
                    }
                }
            }
            Text {
                anchors.centerIn: parent
                visible: albums.count === 0
                width: parent.width * 0.8
                horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                text: "Nog geen eigen muziek. Voeg favorieten toe op de beheerpagina (Media > Muziek); de hoes zoekt de kiosk er zelf bij."
                font.pixelSize: 38 * page.s; color: "#C3CAE0"
            }
        }

        // ---------- Meezingen en karaoke ----------
        LinkGrid { anchors.fill: parent; visible: page.section === "zingen"; items: page.links("zingen") }

        // ---------- Verhalen: voor kinderen en voor volwassenen ----------
        ColumnLayout {
            anchors.fill: parent
            visible: page.section === "verhalen"
            spacing: 16 * page.s
            Text { text: "Voor kinderen"; font.pixelSize: 42 * page.s; font.bold: true; color: page.mint }
            LinkGrid {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(1, Math.ceil(items.length / 2)) * cellHeight
                Layout.maximumHeight: parent.height / 2
                items: page.links("verhalen").filter(l => l.kids)
            }
            Text { text: "Voor volwassenen"; font.pixelSize: 42 * page.s; font.bold: true; color: page.mint }
            LinkGrid { Layout.fillWidth: true; Layout.fillHeight: true; items: page.links("verhalen").filter(l => !l.kids) }
        }

        // ---------- Dieren live ----------
        LinkGrid { anchors.fill: parent; visible: page.section === "dieren"; items: page.links("dieren") }
    }

    // ---------- Speler voor de natuurgeluiden (ook zonder internet) ----------
    QtObject {
        id: nature
        property string key: ""
        property string title: ""
        property int minutes: 30
        readonly property bool playing: player.playbackState === MediaPlayer.PlayingState
        property Timer sleep: Timer { interval: nature.minutes * 60 * 1000; onTriggered: nature.stop() }
        function play(k, t) {
            page.home.stopRadio()
            key = k; title = t
            player.source = "qrc:/sounds/" + k + ".mp3"
            player.play()
            sleep.restart()
            stats.count("relax", "geluid " + t)
            console.info("Nature sound: " + k)
        }
        function stop() { player.stop(); sleep.stop() }
    }
    MediaPlayer {
        id: player
        loops: MediaPlayer.Infinite
        audioOutput: AudioOutput { volume: page.home.radioVolume }
    }
    Component.onDestruction: player.stop()
    Component.onCompleted: console.info("Relax screen opened: links " + agenda.relaxLinks.length + ", favorites " + agenda.favorites.length)
}
