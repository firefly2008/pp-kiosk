// Vraagt hoeveel plaatsen (of rijen) een tegel moet schuiven.
// Pijltjes of D-pad: aantal kiezen, cijfers typen kan ook; Enter/A bevestigt, Esc/B annuleert.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

FocusScope {
    id: picker

    readonly property real unit: height / 100
    property string title
    property string unitName: "plaatsen"
    property int maximum: 1
    property int value: 1
    property Item returnFocusTo: null
    property string typed: ""
    signal accepted(int places)

    anchors.fill: parent
    visible: false
    z: 100

    function open(pickerTitle, pickerUnit, max, focusItem) {
        title = pickerTitle
        unitName = pickerUnit
        maximum = isFinite(max) ? Math.max(1, max) : 1
        value = 1
        typed = ""
        returnFocusTo = focusItem
        visible = true
        forceActiveFocus()
    }

    function close() {
        visible = false
        if (returnFocusTo) {
            returnFocusTo.forceActiveFocus()
        }
    }

    function confirm() {
        const places = value
        close()
        accepted(places)
    }

    function change(delta) {
        typed = ""
        value = Math.max(1, Math.min(maximum, value + delta))
    }

    Keys.onLeftPressed: change(-1)
    Keys.onDownPressed: change(-1)
    Keys.onRightPressed: change(1)
    Keys.onUpPressed: change(1)
    Keys.onReturnPressed: confirm()
    Keys.onEnterPressed: confirm()
    Keys.onEscapePressed: close()
    Keys.onBackPressed: close()
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Menu) {
            close()
            event.accepted = true
        } else if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
            typed = (typed + (event.key - Qt.Key_0)).slice(-3)
            value = Math.max(1, Math.min(maximum, parseInt(typed)))
            event.accepted = true
        } else if (event.key === Qt.Key_Backspace) {
            typed = typed.slice(0, -1)
            value = typed === "" ? 1 : Math.max(1, Math.min(maximum, parseInt(typed)))
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#b0000000"
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onClicked: picker.close() }
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(picker.width * 0.8, picker.unit * 70)
        height: column.height + picker.unit * 7
        radius: picker.unit * 2
        color: "#1d2a40"
        border.color: "#44ffffff"
        border.width: 1
        MouseArea { anchors.fill: parent }

        Column {
            id: column
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: picker.unit * 3.5
            spacing: picker.unit * 2

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                elide: Text.ElideRight
                color: "white"
                font.pixelSize: (picker.unit * 3.2) * appModel.fontScale
                text: picker.title
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                color: "#9fb4d0"
                font.pixelSize: (picker.unit * 2.3) * appModel.fontScale
                text: qsTr("Hoeveel ") + picker.unitName + qsTr("? (1 tot ") + picker.maximum + ")"
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: picker.unit * 4

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: picker.unit * 8; height: width; radius: width / 2
                    color: picker.value > 1 ? "#33ffffff" : "#15ffffff"
                    Text { anchors.centerIn: parent; color: "white"; font.pixelSize: (picker.unit * 4.5) * appModel.fontScale; text: "−"}
                    MouseArea { anchors.fill: parent; onClicked: picker.change(-1) }
                }
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    anchors.verticalCenter: parent.verticalCenter
                    width: picker.unit * 14
                    horizontalAlignment: Text.AlignHCenter
                    color: "white"
                    font.pixelSize: (picker.unit * 9) * appModel.fontScale
                    font.weight: Font.DemiBold
                    text: picker.value
                }
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: picker.unit * 8; height: width; radius: width / 2
                    color: picker.value < picker.maximum ? "#33ffffff" : "#15ffffff"
                    Text { anchors.centerIn: parent; color: "white"; font.pixelSize: (picker.unit * 4.5) * appModel.fontScale; text: "+"}
                    MouseArea { anchors.fill: parent; onClicked: picker.change(1) }
                }
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: picker.unit * 1.5
                FocusButton {
                    unit: picker.unit
                    text: qsTr("Verplaatsen")
                    checked: true
                    onClicked: picker.confirm()
                }
                FocusButton {
                    unit: picker.unit
                    text: qsTr("Annuleren")
                    onClicked: picker.close()
                }
            }

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.horizontalCenter: parent.horizontalCenter
                color: "#a8acb3"
                font.pixelSize: (picker.unit * 1.9) * appModel.fontScale
                text: gamepad.connectedCount > 0 ? qsTr("◀ ▶ aantal kiezen   ·   Ⓐ verplaatsen   ·   Ⓑ annuleren")
                                                 : qsTr("← → aantal kiezen   ·   Enter: verplaatsen   ·   Esc: annuleren")
            }
        }
    }
}
