// Pincode invoeren (kinderslot): vier cijfers met een cijferblok voor controller en afstandsbediening.
// Cijfertoetsen op het toetsenbord werken ook. ask(...) roept onDone(pin) aan; geeft die false terug,
// dan blijft het venster open met de melding qsTr("Onjuiste pincode").
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

FocusScope {
    id: pad

    property string title
    property string message
    property string pin: ""
    property string error: ""
    property var onDone: null
    property Item returnFocusTo: null
    readonly property real unit: height / 100
    readonly property var keys: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "⌫", "0", "OK"]

    anchors.fill: parent
    visible: false
    z: 110

    function ask(padTitle, padMessage, done, focusItem) {
        title = padTitle
        message = padMessage || ""
        onDone = done
        returnFocusTo = focusItem
        pin = ""
        error = ""
        grid.currentIndex = 0
        visible = true
        grid.forceActiveFocus()
    }

    function close() {
        visible = false
        if (returnFocusTo) {
            returnFocusTo.forceActiveFocus()
        }
    }

    function press(key) {
        error = ""
        if (key === "⌫") {
            pin = pin.slice(0, -1)
        } else if (key === "OK") {
            submit()
        } else if (pin.length < 4) {
            pin += key
            if (pin.length === 4) {
                submitTimer.restart()
            }
        }
    }

    function submit() {
        if (pin.length !== 4) {
            error = qsTr("Vul vier cijfers in")
            return
        }
        const done = onDone
        const entered = pin
        const focusBack = returnFocusTo
        // Eerst focus terug, dan done(): die kan zelf een scherm (of dit venster opnieuw) openen
        visible = false
        if (focusBack) {
            focusBack.forceActiveFocus()
        }
        const ok = done ? done(entered) : true
        if (ok === false) {
            returnFocusTo = focusBack
            onDone = done
            pin = ""
            error = parental.waitSeconds() > 0 ? qsTr("Te vaak fout: wacht %1 seconden").arg(parental.waitSeconds()) : qsTr("Onjuiste pincode")
            visible = true
            grid.forceActiveFocus()
            shake.restart()
        }
    }

    // Vierde cijfer: even laten zien, dan controleren
    Timer {
        id: submitTimer
        interval: 250
        onTriggered: pad.submit()
    }

    Rectangle {
        anchors.fill: parent
        color: "#c0000000"
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onClicked: pad.close() }
    }

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: pad.unit * 52
        height: column.height + pad.unit * 8
        radius: pad.unit * 2
        color: "#1d2a40"
        border.color: "#44ffffff"
        border.width: 1
        MouseArea { anchors.fill: parent }

        SequentialAnimation {
            id: shake
            NumberAnimation { target: panel; property: "anchors.horizontalCenterOffset"; to: -pad.unit * 1.5; duration: 50 }
            NumberAnimation { target: panel; property: "anchors.horizontalCenterOffset"; to: pad.unit * 1.5; duration: 70 }
            NumberAnimation { target: panel; property: "anchors.horizontalCenterOffset"; to: 0; duration: 50 }
        }

        Column {
            id: column
            anchors.top: parent.top
            anchors.topMargin: pad.unit * 4
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - pad.unit * 8
            spacing: pad.unit * 2

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: "🔒  " + pad.title
                color: "white"
                font.pixelSize: pad.unit * 3.4 * appModel.fontScale
                font.weight: Font.DemiBold
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                visible: text !== ""
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: pad.message
                color: "#c9ccd1"
                font.pixelSize: pad.unit * 2.4 * appModel.fontScale
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: pad.unit * 2
                Repeater {
                    model: 4
                    Rectangle {
                        required property int index
                        width: pad.unit * 3
                        height: width
                        radius: width / 2
                        color: index < pad.pin.length ? "white" : "transparent"
                        border.color: "white"
                        border.width: pad.unit * 0.3
                    }
                }
            }

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                height: pad.unit * 3
                horizontalAlignment: Text.AlignHCenter
                text: pad.error
                color: "#ff8a80"
                font.pixelSize: pad.unit * 2.4 * appModel.fontScale
            }

            GridView {
                id: grid
                anchors.horizontalCenter: parent.horizontalCenter
                width: pad.unit * 36
                height: pad.unit * 40
                cellWidth: width / 3
                cellHeight: height / 4
                model: pad.keys
                interactive: false
                keyNavigationEnabled: true
                focus: true

                delegate: Item {
                    required property int index
                    required property string modelData
                    width: grid.cellWidth
                    height: grid.cellHeight
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: pad.unit * 0.8
                        radius: pad.unit * 1.4
                        color: grid.activeFocus && grid.currentIndex === index ? "#3d6fb4" : "#2a3954"
                        border.color: grid.activeFocus && grid.currentIndex === index ? "white" : "transparent"
                        border.width: pad.unit * 0.25
                        Text {
                            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                            anchors.centerIn: parent
                            text: modelData
                            color: "white"
                            font.pixelSize: pad.unit * (modelData.length > 1 ? 3 : 4) * appModel.fontScale
                            font.weight: Font.DemiBold
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: { grid.currentIndex = index; pad.press(modelData) }
                        }
                    }
                }

                Keys.onReturnPressed: pad.press(pad.keys[currentIndex])
                Keys.onEnterPressed: pad.press(pad.keys[currentIndex])
                Keys.onPressed: (event) => {
                    if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
                        pad.press(String(event.key - Qt.Key_0))
                        event.accepted = true
                    } else if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) {
                        pad.press("⌫")
                        event.accepted = true
                    } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
                        pad.close()
                        event.accepted = true
                    }
                }
            }
        }
    }
}
