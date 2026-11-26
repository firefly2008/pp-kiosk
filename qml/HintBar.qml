// Hulpregel met knoppen in plaats van jargon. Met een controller: gekleurde rondjes (Ⓐ Ⓑ Ⓧ Ⓨ),
// anders toetsen ("Enter", "Esc"). Gebruik:
//   HintBar { unit: ...; items: [ { pad: "A", key: "Enter", label: qsTr("Openen") }, ... ] }
// pad / key mogen leeg zijn (dan alleen bij de andere bediening tonen).
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

Row {
    id: bar

    property real unit: 10
    property var items: []
    property color textColor: "#d5d8dc"
    readonly property bool padMode: gamepad.connectedCount > 0
    readonly property real size: unit * 2.4

    spacing: unit * 2.4

    function padColor(button) {
        switch (button) {
        case "A": return "#3fae49"
        case "B": return "#e0463e"
        case "X": return "#2f7ee0"
        case "Y": return "#e6b422"
        default: return "#5f6368"
        }
    }

    Repeater {
        model: bar.items.filter(it => bar.padMode ? (it.pad || "") !== "" : (it.key || "") !== "")
        delegate: Row {
            required property var modelData
            spacing: bar.unit * 0.8
            // Knop
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                readonly property string word: bar.padMode ? modelData.pad : modelData.key
                readonly property bool round: bar.padMode && word.length === 1
                height: bar.size * 1.35
                width: round ? height : Math.max(height, keyText.implicitWidth + bar.size * 0.9)
                radius: round ? height / 2 : bar.size * 0.3
                color: round ? bar.padColor(word) : "#33ffffff"
                border.width: round ? 0 : 1
                border.color: "#66ffffff"
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    id: keyText
                    anchors.centerIn: parent
                    color: "white"
                    font.pixelSize: (bar.size * 0.82) * appModel.fontScale
                    font.weight: Font.DemiBold
                    text: parent.word
                }
            }
            // Wat de knop doet
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.verticalCenter: parent.verticalCenter
                color: bar.textColor
                font.pixelSize: (bar.size) * appModel.fontScale
                text: modelData.label
            }
        }
    }
}
