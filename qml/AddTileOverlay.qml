// Formulier om zelf een tegel toe te voegen: een website (streamingdienst) of een programma.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

FocusScope {
    id: overlay

    property Item returnFocusTo: null
    property alias targetText: targetField.text
    signal accepted(string name, string target)
    signal browseRequested()

    anchors.fill: parent
    visible: false
    z: 100

    function open(focusItem) {
        returnFocusTo = focusItem
        nameField.text = ""
        targetField.text = ""
        visible = true
        nameField.forceActiveFocus()
    }

    function close() {
        visible = false
        if (returnFocusTo) {
            returnFocusTo.forceActiveFocus()
        }
    }

    function submit() {
        if (targetField.text.trim() === "") {
            targetField.forceActiveFocus()
            return
        }
        const name = nameField.text
        const target = targetField.text
        close()
        accepted(name, target)
    }

    Keys.onEscapePressed: close()

    Rectangle {
        anchors.fill: parent
        color: "#b0000000"
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; hoverEnabled: true }
    }

    Rectangle {
        id: panel
        readonly property real unit: overlay.height / 100
        anchors.centerIn: parent
        width: Math.min(overlay.width * 0.85, unit * 90)
        height: content.height + unit * 6
        radius: unit * 2
        color: "#1d2a40"
        border.color: "#44ffffff"
        border.width: 1

        Column {
            id: content
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: panel.unit * 3
            spacing: panel.unit * 1.5

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                color: "white"
                font.pixelSize: (panel.unit * 3.4) * appModel.fontScale
                text: qsTr("Tegel toevoegen")
            }

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                color: "#9fb4d0"
                font.pixelSize: (panel.unit * 2.2) * appModel.fontScale
                text: qsTr("Naam")
            }
            Rectangle {
                width: parent.width
                height: panel.unit * 6.5
                radius: panel.unit
                color: "#0f1826"
                border.width: nameField.activeFocus ? 3 : 1
                border.color: nameField.activeFocus ? "#5b9bff" : "#44ffffff"
                TextInput {
                    id: nameField
                    anchors.fill: parent
                    anchors.margins: panel.unit * 1.5
                    verticalAlignment: TextInput.AlignVCenter
                    color: "white"
                    font.pixelSize: (panel.unit * 2.6) * appModel.fontScale
                    clip: true
                    selectByMouse: true
                    KeyNavigation.tab: targetField
                    KeyNavigation.down: targetField
                    onAccepted: targetField.forceActiveFocus()
                }
            }

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                color: "#9fb4d0"
                font.pixelSize: (panel.unit * 2.2) * appModel.fontScale
                text: qsTr("Webadres (bijv. netflix.com) of pad naar een programma")
            }
            Rectangle {
                width: parent.width
                height: panel.unit * 6.5
                radius: panel.unit
                color: "#0f1826"
                border.width: targetField.activeFocus ? 3 : 1
                border.color: targetField.activeFocus ? "#5b9bff" : "#44ffffff"
                TextInput {
                    id: targetField
                    anchors.fill: parent
                    anchors.margins: panel.unit * 1.5
                    verticalAlignment: TextInput.AlignVCenter
                    color: "white"
                    font.pixelSize: (panel.unit * 2.6) * appModel.fontScale
                    clip: true
                    selectByMouse: true
                    KeyNavigation.tab: buttons
                    KeyNavigation.backtab: nameField
                    KeyNavigation.up: nameField
                    KeyNavigation.down: buttons
                    onAccepted: overlay.submit()
                }
            }

            ListView {
                id: buttons
                width: parent.width
                height: panel.unit * 7
                orientation: ListView.Horizontal
                spacing: panel.unit * 1.5
                interactive: false
                keyNavigationEnabled: true
                model: [qsTr("Toevoegen"), qsTr("Programma zoeken…"), qsTr("Annuleren")]
                KeyNavigation.up: targetField
                KeyNavigation.backtab: targetField

                function trigger(index) {
                    if (index === 0) {
                        overlay.submit()
                    } else if (index === 1) {
                        overlay.browseRequested()
                    } else {
                        overlay.close()
                    }
                }

                delegate: Rectangle {
                    id: button
                    required property int index
                    required property string modelData
                    width: (buttons.width - buttons.spacing * 2) / 3
                    height: buttons.height
                    radius: height / 2
                    color: buttons.activeFocus && buttons.currentIndex === index ? "#3d6fb4" : "#22ffffff"
                    border.width: buttons.activeFocus && buttons.currentIndex === index ? 3 : 0
                    border.color: "white"
                    Text {
                        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                        anchors.centerIn: parent
                        color: "white"
                        font.pixelSize: (panel.unit * 2.5) * appModel.fontScale
                        text: button.modelData
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { buttons.currentIndex = button.index; buttons.trigger(button.index) }
                    }
                }

                Keys.onReturnPressed: trigger(currentIndex)
                Keys.onEnterPressed: trigger(currentIndex)
            }
        }
    }
}
