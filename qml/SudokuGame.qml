// Makkelijke sudoku van 4 bij 4: in elke rij, kolom en elk blok van 2x2 staan de cijfers 1 tot en met 4.
// Tik op een leeg vakje en daarna op een cijfer.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts

Item {
    id: su
    required property var g
    signal won()
    property var solution: []
    property var given: []      // true: vast cijfer
    property var values: []     // 0 = leeg
    property int selected: -1
    property int mistakes: 0
    readonly property color good: g.glass ? "#8BE9C0" : "#2E8B57"

    function start() {
        // Een geldige oplossing door rijen, kolommen en cijfers te husselen
        const base = [[1, 2, 3, 4], [3, 4, 1, 2], [2, 1, 4, 3], [4, 3, 2, 1]]
        const sh = a => a.sort(() => Math.random() - 0.5)
        const digits = sh([1, 2, 3, 4])
        const bands = sh([0, 1]), stacks = sh([0, 1])
        const rows = [].concat(...bands.map(b => sh([0, 1]).map(r => b * 2 + r)))
        const cols = [].concat(...stacks.map(b => sh([0, 1]).map(c => b * 2 + c)))
        const sol = []
        for (let r = 0; r < 4; ++r) for (let c = 0; c < 4; ++c) sol.push(digits[base[rows[r]][cols[c]] - 1])
        solution = sol
        const empty = sh([...Array(16).keys()]).slice(0, 7)
        given = sol.map((v, i) => empty.indexOf(i) < 0)
        values = sol.map((v, i) => given[i] ? v : 0)
        selected = -1; mistakes = 0
        console.info("Game started: sudoku 4x4")
    }
    function put(d) {
        if (selected < 0 || given[selected]) return
        if (d !== solution[selected]) { mistakes++; wrongFlash.restart(); return }
        const v = values.slice(); v[selected] = d; values = v
        selected = -1
        if (values.every((x, i) => x === solution[i])) { console.info("Game won: sudoku"); su.won() }
    }
    property bool wrong: false
    SequentialAnimation {
        id: wrongFlash
        PropertyAction { target: su; property: "wrong"; value: true }
        PauseAnimation { duration: 600 }
        PropertyAction { target: su; property: "wrong"; value: false }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 60 * su.g.s
        Item {
            Layout.fillHeight: true
            Layout.preferredWidth: height
            Grid {
                id: board
                anchors.centerIn: parent
                columns: 4
                readonly property real cell: Math.min(parent.width, parent.height) / 4
                Repeater {
                    model: 16
                    delegate: Rectangle {
                        required property int index
                        readonly property int r: Math.floor(index / 4)
                        readonly property int c: index % 4
                        width: board.cell; height: board.cell
                        color: su.selected === index ? (su.wrong ? "#C0392B" : "#F2B33D") : su.given[index] ? su.g.pressBg : su.g.cardBg
                        border.color: su.g.lineColor; border.width: 1
                        // Dikke lijnen tussen de blokken van 2x2
                        Rectangle { visible: c === 1; anchors { right: parent.right; top: parent.top; bottom: parent.bottom } width: 6 * su.g.s; color: su.g.ink; opacity: 0.6 }
                        Rectangle { visible: r === 1; anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 6 * su.g.s; color: su.g.ink; opacity: 0.6 }
                        Text {
                            anchors.centerIn: parent
                            text: su.values[index] ? su.values[index] : ""
                            font.pixelSize: board.cell * 0.55; font.bold: su.given[index]
                            color: su.given[index] ? su.g.ink : su.good
                        }
                        TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: if (!su.given[index]) su.selected = index }
                    }
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 24 * su.g.s
            Text {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: su.selected < 0 ? "Tik op een leeg vakje." : "Welk cijfer hoort hier?"
                font.pixelSize: 46 * su.g.s; font.bold: true; color: su.g.ink
            }
            Text {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: "In elke rij, elke kolom en elk blok staan 1, 2, 3 en 4 precies één keer."
                font.pixelSize: 34 * su.g.s; color: su.g.soft
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: 24 * su.g.s; columnSpacing: 24 * su.g.s
                Repeater {
                    model: [1, 2, 3, 4]
                    delegate: Rectangle {
                        required property int modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 180 * su.g.s
                        radius: 36 * su.g.s
                        opacity: su.selected >= 0 ? 1 : 0.5
                        color: numTap.pressed ? su.g.pressBg : su.g.cardBg
                        border.color: su.g.lineColor; border.width: 2
                        Text { anchors.centerIn: parent; text: modelData; font.pixelSize: 110 * su.g.s; font.bold: true; color: su.g.ink }
                        TapHandler { id: numTap; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: su.put(modelData) }
                    }
                }
            }
            Item { Layout.fillHeight: true }
        }
    }
}
