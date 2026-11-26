// Schermtoetsenbord voor controller en tv-afstandsbediening (pijltjes + Enter).
// Typt in `target` (een TextInput). Rechts van de laatste toets: exitRight().
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

FocusScope {
    id: kb

    property var target: null
    property real unit: 10
    property int currentRow: 1
    property int currentCol: 0
    readonly property var rows: [
        ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"],
        ["q", "w", "e", "r", "t", "y", "u", "i", "o", "p"],
        ["a", "s", "d", "f", "g", "h", "j", "k", "l", "'"],
        ["z", "x", "c", "v", "b", "n", "m", "-", ".", "&"],
        ["Spatie", "⌫", "Wissen", "OK"]
    ]
    readonly property real keyWidth: unit * 5.6
    readonly property real keyHeight: unit * 5.6
    signal exitRight()
    signal exitUp()
    signal typed()
    signal accepted()      // OK-toets: bevestigen (zoals Enter op het eerste resultaat)

    width: keyWidth * 10
    height: keyHeight * rows.length

    function keyWidthFor(row, col) {
        return row === 4 ? (col === 0 ? keyWidth * 3.5 : col === 3 ? keyWidth * 2.5 : keyWidth * 2) : keyWidth
    }

    function press(label) {
        if (!target) {
            return
        }
        if (label === "⌫") {
            if (target.text.length > 0) {
                target.text = target.text.slice(0, -1)
            }
        } else if (label === "Wissen") {
            target.text = ""
        } else if (label === "OK") {
            accepted()
            return
        } else {
            target.text += label === "Spatie" ? " " : label
        }
        typed()
    }

    function clampCol() {
        currentCol = Math.min(currentCol, rows[currentRow].length - 1)
    }

    // Gewoon toetsenbord: letters en Backspace gaan rechtstreeks in het zoekveld
    Keys.onPressed: (event) => {
        if (!target) {
            return
        }
        if (event.key === Qt.Key_Backspace) {
            press("⌫")
            event.accepted = true
        } else if (event.text.length === 1 && event.text >= " " && event.key !== Qt.Key_Return && event.key !== Qt.Key_Enter
                   && !(event.modifiers & Qt.ControlModifier)) {
            target.text += event.text
            typed()
            event.accepted = true
        }
    }
    Keys.onLeftPressed: currentCol = Math.max(0, currentCol - 1)
    Keys.onRightPressed: {
        if (currentCol >= rows[currentRow].length - 1) {
            exitRight()
        } else {
            currentCol += 1
        }
    }
    Keys.onUpPressed: {
        if (currentRow === 0) {
            exitUp()
            return
        }
        if (currentRow === 4) {
            currentCol = currentCol === 0 ? 1 : currentCol === 1 ? 4 : currentCol === 2 ? 6 : 8
        }
        currentRow -= 1
    }
    Keys.onDownPressed: {
        if (currentRow === 3) {
            currentCol = currentCol < 4 ? 0 : currentCol < 6 ? 1 : currentCol < 8 ? 2 : 3
        }
        currentRow = Math.min(rows.length - 1, currentRow + 1)
        clampCol()
    }
    Keys.onReturnPressed: press(rows[currentRow][currentCol])
    Keys.onEnterPressed: press(rows[currentRow][currentCol])

    Column {
        Repeater {
            model: kb.rows.length
            Row {
                id: keyRow
                required property int index
                readonly property int rowIndex: index
                Repeater {
                    model: kb.rows[keyRow.rowIndex]
                    Item {
                        required property int index
                        required property string modelData
                        readonly property bool current: kb.activeFocus && kb.currentRow === keyRow.rowIndex && kb.currentCol === index
                        width: kb.keyWidthFor(keyRow.rowIndex, index)
                        height: kb.keyHeight
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: kb.unit * 0.35
                            radius: kb.unit * 0.8
                            color: parent.current ? "#3d6fb4" : modelData === "OK" ? "#2f6b3a" : "#262b33"
                            border.color: parent.current ? "white" : "#3a3f48"
                            border.width: parent.current ? kb.unit * 0.25 : 1
                            Text {
                                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                                anchors.centerIn: parent
                                text: qsTr(modelData)
                                color: "white"
                                font.pixelSize: kb.unit * (modelData.length > 1 ? 2.2 : 2.8) * appModel.fontScale
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    kb.currentRow = keyRow.rowIndex
                                    kb.currentCol = index
                                    kb.press(modelData)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
