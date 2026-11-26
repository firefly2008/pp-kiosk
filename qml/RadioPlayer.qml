// Radio luisteren in Kijkluister zelf: schermvullend, met het logo van de zender groot in beeld.
// Links/rechts: andere zender. Omhoog/omlaag: volume. Enter: pauze.
// Esc/B: terug naar het startscherm, de radio speelt door. X/Menu/Delete: stoppen. Y: slaaptimer.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtMultimedia

FocusScope {
    id: radioPlayer

    readonly property real unit: height / 100
    property Item returnFocusTo: null
    property int row: -1
    property int streamIndex: 0
    property string errorText: ""
    property string nowPlaying: ""
    readonly property string accent: row >= 0 ? radio.accentAt(row) : "#22252b"
    readonly property string logo: row >= 0 ? radio.iconAt(row + radio.revision * 0) : ""
    readonly property bool loading: errorText === "" && (player.mediaStatus === MediaPlayer.LoadingMedia
                                     || player.mediaStatus === MediaPlayer.StalledMedia
                                     || player.mediaStatus === MediaPlayer.BufferingMedia
                                     || (player.playbackState === MediaPlayer.PlayingState && player.mediaStatus === MediaPlayer.NoMedia))
    readonly property bool playing: player.playbackState === MediaPlayer.PlayingState && !loading && errorText === ""
    readonly property bool paused: player.playbackState === MediaPlayer.PausedState
    // Speelt (of laadt) er een zender, ook als het scherm dicht is
    readonly property bool active: row >= 0 && player.playbackState !== MediaPlayer.StoppedState
    readonly property bool inBackground: active && !visible
    signal sleepRequested()
    // Nachtmodus: radio zachter
    property real maxVolume: 1.0
    onMaxVolumeChanged: if (audio.volume > maxVolume) audio.volume = maxVolume
    property string sleepText: ""
    // Schermbeveiliging: na een minuut zonder bediening zweeft de tegel over het scherm
    // en de rest van het scherm wordt vaag (tegen inbranden en voor een rustig beeld)
    property int idleMs: 60000
    property bool drifting: false
    property real driftX: 0
    property real driftY: 0
    property real driftVx: unit * 0.16
    property real driftVy: unit * 0.11
    function wake() {
        if (drifting) console.info("Radio drift", false)
        drifting = false
        idleTimer.restart()
    }

    anchors.fill: parent
    visible: false
    z: 60

    onActiveFocusChanged: {
        if (visible && !activeFocus) {
            Qt.callLater(() => { if (radioPlayer.visible && !radioPlayer.activeFocus && !menu.visible) radioPlayer.forceActiveFocus() })
        }
    }

    onVisibleChanged: { drifting = false; if (visible) idleTimer.restart(); else idleTimer.stop() }

    Timer {
        id: idleTimer
        interval: radioPlayer.idleMs
        onTriggered: {
            if (radioPlayer.visible && radioPlayer.active && !menu.visible) {
                radioPlayer.drifting = true
                console.info("Radio drift", true)
            }
        }
    }
    Timer {
        interval: 33
        repeat: true
        running: radioPlayer.drifting
        onTriggered: {
            const maxX = Math.max(0, (radioPlayer.width - tileBox.width) / 2 - radioPlayer.unit * 2)
            const maxY = Math.max(0, (radioPlayer.height - tileBox.height) / 2 - radioPlayer.unit * 8)
            let x = radioPlayer.driftX + radioPlayer.driftVx
            let y = radioPlayer.driftY + radioPlayer.driftVy
            if (x > maxX) { x = maxX; radioPlayer.driftVx = -Math.abs(radioPlayer.driftVx) }
            if (x < -maxX) { x = -maxX; radioPlayer.driftVx = Math.abs(radioPlayer.driftVx) }
            // De tegel staat standaard iets boven het midden (-6): daar hoort de bodem bij
            if (y > maxY) { y = maxY; radioPlayer.driftVy = -Math.abs(radioPlayer.driftVy) }
            if (y < -maxY) { y = -maxY; radioPlayer.driftVy = Math.abs(radioPlayer.driftVy) }
            radioPlayer.driftX = x
            radioPlayer.driftY = y
        }
    }

    function open(stationRow, focusItem) {
        returnFocusTo = focusItem
        visible = true
        forceActiveFocus()
        if (stationRow !== row || !active) {
            tune(stationRow)
        }
    }

    // Weer tonen terwijl hij op de achtergrond speelt
    function show(focusItem) {
        returnFocusTo = focusItem
        visible = true
        forceActiveFocus()
    }

    // Terug naar het startscherm; de radio speelt door
    function close() {
        if (!active) {
            stopPlayback()
            return
        }
        console.info("Radio in background", radio.nameAt(row))
        visible = false
        if (returnFocusTo) {
            returnFocusTo.forceActiveFocus()
        }
    }

    function stopPlayback() {
        console.info("Radio closed")
        stallTimer.stop()
        player.stop()
        player.source = ""
        const wasVisible = visible
        visible = false
        row = -1
        if (wasVisible && returnFocusTo) {
            returnFocusTo.forceActiveFocus()
        }
    }

    function tune(stationRow) {
        row = radio.isStation(stationRow) ? stationRow : radio.nextStation(stationRow, 1)
        streamIndex = 0
        playStream()
        radio.requestProgramme(row)
    }

    function playStream() {
        errorText = ""
        nowPlaying = ""
        const streams = radio.streamsAt(row)
        console.info("Radio play", radio.nameAt(row), streams[streamIndex])
        player.stop()
        player.source = streams[streamIndex]
        player.play()
        stallTimer.restart()
    }

    // Lukt een adres niet: het volgende proberen, daarna een melding
    function tryNext(reason) {
        const streams = radio.streamsAt(row)
        console.info("Radio stream failed", streams[streamIndex], reason)
        if (streamIndex + 1 < streams.length) {
            streamIndex += 1
            playStream()
        } else {
            player.stop()
            errorText = radio.nameAt(row) + qsTr(" is nu niet te beluisteren.")
        }
    }

    function togglePause() {
        if (errorText !== "") {
            streamIndex = 0
            playStream()
        } else if (player.playbackState === MediaPlayer.PlayingState) {
            player.pause()
        } else {
            // Live radio: na een pauze weer bij "nu" beginnen
            playStream()
        }
    }

    function changeVolume(delta) {
        audio.volume = Math.max(0, Math.min(maxVolume, Math.round((audio.volume + delta) * 20) / 20))
        volumeFlash.restart()
    }

    MediaPlayer {
        id: player
        audioOutput: AudioOutput {
            id: audio
            volume: 0.8
        }
        onErrorOccurred: (error, message) => radioPlayer.tryNext(message)
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.BufferedMedia) {
                stallTimer.stop()
                console.info("Radio playing", radio.nameAt(radioPlayer.row), source)
            } else if (mediaStatus === MediaPlayer.InvalidMedia) {
                radioPlayer.tryNext("invalid media")
            }
        }
        onMetaDataChanged: {
            const title = metaData.stringValue(MediaMetaData.Title)
            const artist = metaData.stringValue(MediaMetaData.ContributingArtist)
            const text = (artist && title) ? artist + " – " + title : (title || "")
            // Sommige zenders sturen hun eigen naam of een slogan als titel
            radioPlayer.nowPlaying = text.toLowerCase().indexOf(radio.nameAt(radioPlayer.row).toLowerCase()) >= 0 ? "" : text
        }
    }

    // Programma-info (NPO-zenders) elke minuut verversen
    Timer {
        interval: 60000
        repeat: true
        running: radioPlayer.visible && radioPlayer.row >= 0
        onTriggered: radio.requestProgramme(radioPlayer.row)
    }

    // Blijft het te lang laden: volgend adres
    Timer {
        id: stallTimer
        interval: 15000
        onTriggered: if (radioPlayer.visible && radioPlayer.loading) radioPlayer.tryNext("timeout")
    }

    Keys.onLeftPressed: { wake(); tune(radio.nextStation(row, -1)) }
    Keys.onRightPressed: { wake(); tune(radio.nextStation(row, 1)) }
    Keys.onUpPressed: { wake(); changeVolume(0.05) }
    Keys.onDownPressed: { wake(); changeVolume(-0.05) }
    Keys.onPressed: (event) => {
        wake()
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back || event.key === Qt.Key_Backspace) {
            close()
            event.accepted = true
        } else if (event.key === Qt.Key_Menu || event.key === Qt.Key_Delete || event.key === Qt.Key_X || event.key === Qt.Key_MediaStop) {
            stopPlayback()
            event.accepted = true
        } else if (event.key === Qt.Key_I || event.key === Qt.Key_T) {
            sleepRequested()
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space
                   || event.key === Qt.Key_MediaPlay || event.key === Qt.Key_MediaTogglePlayPause) {
            togglePause()
            event.accepted = true
        } else if (event.key === Qt.Key_MediaNext || event.key === Qt.Key_PageDown) {
            tune(radio.nextStation(row, 1))
            event.accepted = true
        } else if (event.key === Qt.Key_MediaPrevious || event.key === Qt.Key_PageUp) {
            tune(radio.nextStation(row, -1))
            event.accepted = true
        } else if (event.key === Qt.Key_VolumeUp) {
            changeVolume(0.05)
            event.accepted = true
        } else if (event.key === Qt.Key_VolumeDown) {
            changeVolume(-0.05)
            event.accepted = true
        }
    }

    // Achtergrond: de kleur van de zender, van boven naar beneden donkerder
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.darker(radioPlayer.accent, 1.1) }
            GradientStop { position: 0.75; color: "#0b0c0f" }
        }
        Behavior on gradient { enabled: false }
    }

    // Schermbeveiliging: achtergrond vervaagt naar bijna zwart
    Rectangle {
        anchors.fill: parent
        color: "#0b0c0f"
        opacity: radioPlayer.drifting ? 0.82 : 0
        Behavior on opacity { NumberAnimation { duration: 1500 } }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onPositionChanged: radioPlayer.wake()
        onClicked: { radioPlayer.wake(); radioPlayer.forceActiveFocus() }
    }

    // Titel linksboven
    Text {
        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
        x: radioPlayer.unit * 4
        y: radioPlayer.unit * 4
        text: qsTr("Radio")
        opacity: radioPlayer.drifting ? 0 : 1
        Behavior on opacity { NumberAnimation { duration: 1500 } }
        color: "#ffffff"
        font.pixelSize: radioPlayer.unit * 4 * appModel.fontScale
        font.weight: Font.DemiBold
    }

    // Midden: logo, naam, status
    Column {
        id: tileBox
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.horizontalCenterOffset: radioPlayer.driftX
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -radioPlayer.unit * 6 + radioPlayer.driftY
        spacing: radioPlayer.unit * 2.4
        // Terug naar het midden na het wakker worden
        Behavior on anchors.horizontalCenterOffset { enabled: !radioPlayer.drifting; NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
        Behavior on anchors.verticalCenterOffset { enabled: !radioPlayer.drifting; NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: radioPlayer.unit * 26
            height: width
            radius: width * 0.14
            color: "#ffffff"
            Image {
                anchors.centerIn: parent
                width: parent.width * 0.72
                height: width
                source: radioPlayer.logo.startsWith("glyph:") ? "" : radioPlayer.logo
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.centerIn: parent
                visible: radioPlayer.logo.startsWith("glyph:")
                text: radioPlayer.logo.substring(6)
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: parent.width * 0.5
                color: radioPlayer.accent
            }
        }

        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            anchors.horizontalCenter: parent.horizontalCenter
            text: radioPlayer.row >= 0 ? radio.nameAt(radioPlayer.row) : ""
            color: "#ffffff"
            font.pixelSize: radioPlayer.unit * 6 * appModel.fontScale
            font.weight: Font.Bold
        }

        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            anchors.horizontalCenter: parent.horizontalCenter
            text: radioPlayer.row >= 0 ? radio.subtitleAt(radioPlayer.row) : ""
            color: "#c9ccd1"
            font.pixelSize: radioPlayer.unit * 2.8 * appModel.fontScale
        }

        // Programma van de zender (NPO Radio): nu en straks
        Column {
            id: programmeBox
            readonly property var info: radio.programme
            readonly property bool shown: info.title !== undefined && info.title !== "" && info.station === (radioPlayer.row >= 0 ? radio.nameAt(radioPlayer.row) : "")
            anchors.horizontalCenter: parent.horizontalCenter
            visible: shown
            spacing: radioPlayer.unit * 0.4
            Text {
                textFormat: Text.PlainText
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(implicitWidth, radioPlayer.width * 0.8)
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                text: programmeBox.shown ? programmeBox.info.title + (programmeBox.info.presenters ? "  ·  " + programmeBox.info.presenters : "") : ""
                color: "#ffffff"
                font.pixelSize: radioPlayer.unit * 3.2 * appModel.fontScale
                font.weight: Font.DemiBold
            }
            Text {
                textFormat: Text.PlainText
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(implicitWidth, radioPlayer.width * 0.8)
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                visible: programmeBox.shown && programmeBox.info.nextTitle !== undefined && programmeBox.info.nextTitle !== ""
                text: visible ? qsTr("Straks %1: %2").arg(Qt.formatDateTime(new Date(programmeBox.info.nextStart * 1000), "HH:mm")).arg(programmeBox.info.nextTitle) : ""
                color: "#aeb3ba"
                font.pixelSize: radioPlayer.unit * 2.4 * appModel.fontScale
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: radioPlayer.unit * 1.2
            height: radioPlayer.unit * 4

            // Live-stip of laad-bolletjes
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                visible: radioPlayer.playing
                width: radioPlayer.unit * 1.6
                height: width
                radius: width / 2
                color: "#e0463e"
                SequentialAnimation on opacity {
                    running: radioPlayer.playing
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 900 }
                    NumberAnimation { to: 1.0; duration: 900 }
                }
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.verticalCenter: parent.verticalCenter
                color: radioPlayer.errorText !== "" ? "#ff8a80" : "#ffffff"
                font.pixelSize: radioPlayer.unit * 3 * appModel.fontScale
                text: radioPlayer.errorText !== "" ? radioPlayer.errorText
                    : radioPlayer.paused ? qsTr("Gepauzeerd")
                    : radioPlayer.loading ? qsTr("Verbinden…")
                    : radioPlayer.playing ? qsTr("Live") : ""
            }
        }

        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            anchors.horizontalCenter: parent.horizontalCenter
            visible: radioPlayer.nowPlaying !== "" && radioPlayer.playing
            width: Math.min(implicitWidth, radioPlayer.width * 0.8)
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Nu: ") + radioPlayer.nowPlaying
            color: "#e6e8eb"
            font.pixelSize: radioPlayer.unit * 2.8 * appModel.fontScale
        }
    }

    // Volume: verschijnt even bij omhoog/omlaag
    Rectangle {
        id: volumeBox
        anchors.right: parent.right
        anchors.rightMargin: radioPlayer.unit * 4
        anchors.verticalCenter: parent.verticalCenter
        width: radioPlayer.unit * 6
        height: radioPlayer.unit * 34
        radius: radioPlayer.unit * 1.4
        color: "#cc16181d"
        opacity: volumeFlash.running && !radioPlayer.drifting ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: radioPlayer.unit * 5
            width: radioPlayer.unit * 1.2
            height: parent.height - radioPlayer.unit * 8
            radius: width / 2
            color: "#40ffffff"
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: parent.height * audio.volume
                radius: width / 2
                color: "#ffffff"
            }
        }
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: radioPlayer.unit * 1.4
            text: Math.round(audio.volume * 100) + "%"
            color: "#ffffff"
            font.pixelSize: radioPlayer.unit * 2 * appModel.fontScale
        }
        Timer { id: volumeFlash; interval: 1800 }
    }

    // Onderaan: alle zenders, de huidige uitgelicht
    Row {
        id: strip
        opacity: radioPlayer.drifting ? 0 : 1
        Behavior on opacity { NumberAnimation { duration: 1500 } }
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: hints.top
        anchors.bottomMargin: radioPlayer.unit * 3
        spacing: radioPlayer.unit * 1.6

        Repeater {
            model: radio.count
            delegate: Rectangle {
                required property int index
                readonly property string name: radio.nameAt(index)
                readonly property string iconSource: radio.iconAt(index + radio.revision * 0)
                readonly property bool current: index === radioPlayer.row
                visible: radio.isStation(index + radio.revision * 0)
                width: visible ? radioPlayer.unit * (current ? 10 : 8) * Math.min(1, 10 / Math.max(1, radio.stationCount)) : 0
                height: width
                radius: width * 0.16
                anchors.verticalCenter: parent ? parent.verticalCenter : undefined
                color: "#ffffff"
                opacity: current ? 1 : 0.55
                border.width: current ? radioPlayer.unit * 0.4 : 0
                border.color: "#ffffff"
                Behavior on width { NumberAnimation { duration: 150 } }
                Image {
                    anchors.centerIn: parent
                    width: parent.width * 0.74
                    height: width
                    source: iconSource.startsWith("glyph:") ? "" : iconSource
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                }
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    anchors.centerIn: parent
                    visible: iconSource.startsWith("glyph:")
                    width: parent.width * 0.9
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    text: name
                    color: "#16181d"
                    font.pixelSize: parent.width * 0.16
                    font.weight: Font.DemiBold
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: radioPlayer.tune(index)
                }
            }
        }
    }

    HintBar {
        id: hints
        opacity: radioPlayer.drifting ? 0 : 1
        Behavior on opacity { NumberAnimation { duration: 1500 } }
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: radioPlayer.unit * 3
        unit: radioPlayer.unit
        items: [
            { pad: "◀ ▶", key: "◀ ▶", label: qsTr("Andere zender") },
            { pad: "▲ ▼", key: "▲ ▼", label: qsTr("Volume") },
            { pad: "A", key: qsTr("Enter"), label: radioPlayer.paused ? qsTr("Verder") : qsTr("Pauze") },
            { pad: "Y", key: "T", label: radioPlayer.sleepText !== "" ? qsTr("Slaaptimer: ") + radioPlayer.sleepText : qsTr("Slaaptimer") },
            { pad: "B", key: qsTr("Esc"), label: qsTr("Terug (speelt door)") },
            { pad: "X", key: qsTr("Del"), label: qsTr("Stoppen") }
        ]
    }
}
