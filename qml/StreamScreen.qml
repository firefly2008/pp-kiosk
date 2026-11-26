// Kijk & luister: tv en video (websites), radio (speelt hier) en podcasts/muziek (websites).
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts
import QtMultimedia

Rectangle {
    id: page
    color: "#F7F4EE"
    required property real s
    signal back()

    readonly property color ink: "#1F2A44"
    readonly property color soft: "#5B6478"
    property string section: "video"
    property int radioRow: -1

    readonly property var sections: [
        { key: "video", title: "Tv en video", icon: "📺", model: webVideo },
        { key: "radio", title: "Radio", icon: "📻", model: radio },
        { key: "podcasts", title: "Podcasts", icon: "🎙️", model: webPodcasts },
        { key: "muziek", title: "Muziek", icon: "🎵", model: webMusic }
    ]
    readonly property var current: sections.find(x => x.key === section)

    MediaPlayer {
        id: player
        audioOutput: AudioOutput { volume: 0.8 }
        onErrorOccurred: (error, text) => console.warn("Radio error: " + text)
        onPlaybackStateChanged: console.info("Radio state " + playbackState)
    }

    function playRadio(row) {
        const streams = radio.streamsAt(row)
        if (!streams || streams.length === 0) return
        radioRow = row
        player.source = streams[0]
        player.play()
        console.info("Radio playing: " + radio.nameAt(row))
    }
    function stopRadio() { player.stop(); radioRow = -1; saver = false }

    function open(model, row) {
        if (model === radio) {
            if (radio.isStation(row)) playRadio(row)
            return
        }
        stopRadio()
        model.launch(row)
        homeBridge.externalLaunched()
        console.info("Stream opened: " + model.nameAt(row))
    }

    // ---------- Kop ----------
    RowLayout {
        id: top
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 40 * page.s }
        height: 120 * page.s
        spacing: 24 * page.s

        Rectangle {
            Layout.preferredWidth: 220 * page.s; Layout.preferredHeight: 100 * page.s
            radius: 24 * page.s
            color: backTap.pressed ? "#DDE3EA" : "#FFFFFF"
            border.color: "#14000000"; border.width: 2
            Text { anchors.centerIn: parent; text: "‹  Terug"; font.pixelSize: 40 * page.s; font.bold: true; color: page.ink }
            TapHandler { id: backTap; onTapped: { page.stopRadio(); page.back() } }
        }
        Text {
            Layout.fillWidth: true
            text: "🎧  Kijk & luister"
            font.pixelSize: 64 * page.s; font.bold: true; color: page.ink
        }
    }

    // ---------- Soorten ----------
    Row {
        id: tabs
        anchors { left: parent.left; top: top.bottom; leftMargin: 40 * page.s; topMargin: 16 * page.s }
        spacing: 20 * page.s
        Repeater {
            model: page.sections
            delegate: Rectangle {
                required property var modelData
                readonly property bool on: page.section === modelData.key
                width: 380 * page.s; height: 110 * page.s
                radius: 28 * page.s
                color: on ? "#2E8B57" : (tabTap.pressed ? "#DDE3EA" : "#FFFFFF")
                border.color: "#14000000"; border.width: on ? 0 : 2
                Row {
                    anchors.centerIn: parent
                    spacing: 16 * page.s
                    Text { text: modelData.icon; font.family: Qt.platform.os === "windows" ? "Segoe UI Emoji" : "Noto Color Emoji"; font.pixelSize: 52 * page.s; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: modelData.title; font.pixelSize: 40 * page.s; font.bold: true; color: on ? "#FFFFFF" : page.ink; anchors.verticalCenter: parent.verticalCenter }
                }
                TapHandler { id: tabTap; onTapped: page.section = modelData.key }
            }
        }
    }

    // ---------- Tegels ----------
    GridView {
        id: grid
        anchors { left: parent.left; right: parent.right; top: tabs.bottom; bottom: nowPlaying.visible ? nowPlaying.top : parent.bottom; margins: 40 * page.s; topMargin: 32 * page.s }
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        readonly property int columns: 5
        cellWidth: width / columns
        cellHeight: cellWidth * 0.72
        model: page.current.model

        delegate: Item {
            required property int index
            required property string name
            required property var iconSource
            required property var accent
            readonly property bool isAdd: page.current.model === radio && !radio.isStation(index)
            readonly property bool playing: page.current.model === radio && page.radioRow === index
            width: grid.cellWidth; height: grid.cellHeight
            visible: !isAdd

            Rectangle {
                anchors { fill: parent; margins: 12 * page.s }
                radius: 28 * page.s
                color: tileTap.pressed ? Qt.darker(String(accent || "#3A4A6B"), 1.2) : String(accent || "#3A4A6B")
                border.color: playing ? "#F2B33D" : "transparent"
                border.width: 8 * page.s

                Image {
                    id: logo
                    anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: parent.height * 0.14 }
                    width: parent.width * 0.42; height: parent.height * 0.42
                    fillMode: Image.PreserveAspectFit
                    source: String(iconSource || "").startsWith("glyph:") ? "" : (iconSource || "")
                    asynchronous: true
                    sourceSize.width: 256
                }
                Text {
                    anchors.centerIn: logo
                    visible: logo.status !== Image.Ready
                    text: name.charAt(0)
                    font.pixelSize: parent.height * 0.32; font.bold: true; color: "#FFFFFF"
                }
                Text {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 16 * page.s }
                    horizontalAlignment: Text.AlignHCenter
                    text: (playing ? "▶ " : "") + name
                    font.pixelSize: 34 * page.s; font.bold: true; color: "#FFFFFF"
                    elide: Text.ElideRight
                    style: Text.Raised; styleColor: "#40000000"
                }
                TapHandler { id: tileTap; onTapped: page.open(page.current.model, index) }
            }
        }
    }

    // ---------- Nu op de radio ----------
    Rectangle {
        id: nowPlaying
        visible: page.radioRow >= 0
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 40 * page.s }
        height: 140 * page.s
        radius: 28 * page.s
        color: "#1F2A44"
        RowLayout {
            anchors { fill: parent; leftMargin: 32 * page.s; rightMargin: 24 * page.s }
            spacing: 24 * page.s
            Text {
                Layout.fillWidth: true
                text: page.radioRow >= 0 ? "📻  " + radio.nameAt(page.radioRow) + (player.playbackState === MediaPlayer.PlayingState ? "" : "  (laden…)") : ""
                font.pixelSize: 44 * page.s; font.bold: true; color: "#FFFFFF"
                elide: Text.ElideRight
            }
            Rectangle {
                Layout.preferredWidth: 260 * page.s; Layout.preferredHeight: 96 * page.s
                radius: 24 * page.s
                color: stopTap.pressed ? "#B03A2E" : "#E74C3C"
                Text { anchors.centerIn: parent; text: "■  Stop"; font.pixelSize: 40 * page.s; font.bold: true; color: "#FFFFFF" }
                TapHandler { id: stopTap; onTapped: page.stopRadio() }
            }
        }
    }

    // ---------- Radio-screensaver: de zender zweeft over het scherm ----------
    property bool saver: false
    onSaverChanged: console.info(saver ? "Radio screensaver started" : "Radio screensaver stopped")
    Timer {
        interval: 5000; running: page.radioRow >= 0 && !page.saver; repeat: true
        onTriggered: {
            const min = agenda.settings.screensaverMinutes || 5
            if (activity.idleSeconds() >= min * 60) page.saver = true
        }
    }
    Connections {
        target: activity
        function onTouched() { page.saver = false }
    }
    Rectangle {
        id: saverLayer
        anchors.fill: parent
        visible: page.saver && page.radioRow >= 0
        z: 50
        gradient: Gradient {
            GradientStop { position: 0; color: "#0E1630" }
            GradientStop { position: 1; color: "#05070F" }
        }
        // Tik ergens: screensaver weg, radio speelt door
        MouseArea { anchors.fill: parent; onPressed: page.saver = false }

        property real vx: 2.2 * page.s
        property real vy: 1.6 * page.s
        property color accent: page.radioRow >= 0 ? String(radio.accentAt(page.radioRow) || "#F2B33D") : "#F2B33D"
        readonly property var accents: ["#F2B33D", "#E85D75", "#4FC3F7", "#8CC084", "#C3B1E1", "#FF8A3D"]
        property int bounce: 0

        Rectangle {
            id: floater
            width: 720 * page.s; height: 300 * page.s
            radius: 48 * page.s
            color: "#1B2547"
            border.color: saverLayer.bounce > 0 ? saverLayer.accents[saverLayer.bounce % saverLayer.accents.length] : saverLayer.accent
            border.width: 8 * page.s
            x: (saverLayer.width - width) / 2; y: (saverLayer.height - height) / 2
            RowLayout {
                anchors { fill: parent; margins: 36 * page.s }
                spacing: 32 * page.s
                Rectangle {
                    Layout.preferredWidth: 200 * page.s; Layout.preferredHeight: 200 * page.s
                    radius: 36 * page.s
                    color: saverLayer.accent
                    Image {
                        id: saverLogo
                        anchors { fill: parent; margins: 24 * page.s }
                        fillMode: Image.PreserveAspectFit
                        source: page.radioRow >= 0 && !String(radio.iconAt(page.radioRow)).startsWith("glyph:") ? radio.iconAt(page.radioRow) : ""
                        sourceSize.width: 256
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: saverLogo.status !== Image.Ready
                        text: "📻"
                        font.family: Qt.platform.os === "windows" ? "Segoe UI Emoji" : "Noto Color Emoji"
                        font.pixelSize: 110 * page.s
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8 * page.s
                    Text {
                        Layout.fillWidth: true
                        text: page.radioRow >= 0 ? radio.nameAt(page.radioRow) : ""
                        font.pixelSize: 64 * page.s; font.bold: true; color: "#FFFFFF"
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "♪  Nu op de radio"
                        font.pixelSize: 38 * page.s; color: "#AEB8D6"
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
            anchors { right: parent.right; bottom: parent.bottom; margins: 40 * page.s }
            text: "Tik op het scherm om terug te gaan"
            font.pixelSize: 30 * page.s; color: "#5B6688"
        }
    }

    Component.onCompleted: console.info("Stream screen opened; video " + webVideo.count + " radio " + radio.count + " podcasts " + webPodcasts.count)
}
