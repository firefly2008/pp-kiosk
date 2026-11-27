// Woordzoeker: 5 woorden in een raster van 9 bij 9 (alleen van links naar rechts en van boven naar beneden).
// Tik op de eerste letter en dan op de laatste letter van een woord.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts

Item {
    id: ws
    required property var g
    signal won()
    readonly property int n: 9
    property var letters: []        // n*n letters
    property var words: []          // { word, cells: [index...], found }
    property int startCell: -1
    property string theme: ""
    readonly property color good: g.glass ? "#8BE9C0" : "#2E8B57"

    readonly property var themes: ({
        "Dieren": ["HOND", "KAT", "KOE", "PAARD", "SCHAAP", "EEND", "VARKEN", "KONIJN", "GEIT", "MUIS", "VOGEL", "VIS"],
        "Eten": ["BROOD", "KAAS", "APPEL", "SOEP", "PEER", "MELK", "TAART", "BOTER", "EIEREN", "PATAT", "THEE", "KOFFIE"],
        "Natuur": ["BOOM", "BLOEM", "ZON", "REGEN", "WOLK", "ZEE", "BOS", "GRAS", "STRAND", "BERG", "MAAN", "STER"],
        "In huis": ["DEUR", "RAAM", "STOEL", "TAFEL", "BED", "LAMP", "KAST", "BANK", "KLOK", "SPIEGEL", "TAPIJT", "BAD"]
    })

    function start() {
        const names = Object.keys(themes)
        theme = names[Math.floor(Math.random() * names.length)]
        for (let attempt = 0; attempt < 50; ++attempt) {
            if (build(themes[theme])) break
        }
        startCell = -1
        console.info("Game started: word search " + theme + " " + words.length + " words")
    }
    function build(list) {
        const grid = new Array(n * n).fill("")
        const pool = list.slice().sort(() => Math.random() - 0.5)
        const placed = []
        for (const w of pool) {
            if (placed.length >= 5) break
            let ok = false
            for (let t = 0; t < 80 && !ok; ++t) {
                const down = Math.random() < 0.5
                const r = Math.floor(Math.random() * (down ? n - w.length + 1 : n))
                const c = Math.floor(Math.random() * (down ? n : n - w.length + 1))
                const cells = []
                for (let k = 0; k < w.length; ++k) cells.push(down ? (r + k) * n + c : r * n + c + k)
                if (cells.every((cell, k) => grid[cell] === "" || grid[cell] === w[k])) {
                    cells.forEach((cell, k) => grid[cell] = w[k])
                    placed.push({ word: w, cells: cells, found: false })
                    ok = true
                }
            }
        }
        if (placed.length < 5) return false
        const abc = "ABCDEEEEFGHIKLMNNOOPRSSTTUVWZ"
        letters = grid.map(x => x !== "" ? x : abc[Math.floor(Math.random() * abc.length)])
        words = placed
        return true
    }
    function tap(cell) {
        if (startCell < 0) { startCell = cell; return }
        const a = startCell
        startCell = -1
        for (let i = 0; i < words.length; ++i) {
            const w = words[i]
            const first = w.cells[0], last = w.cells[w.cells.length - 1]
            if (!w.found && ((a === first && cell === last) || (a === last && cell === first))) {
                const copy = words.slice()
                copy[i] = Object.assign({}, w, { found: true })
                words = copy
                if (agenda.settings.readAloud !== false) speaker.say(w.word.toLowerCase())
                if (words.every(x => x.found)) { console.info("Game won: word search"); ws.won() }
                return
            }
        }
    }
    function isFound(cell) { return words.some(w => w.found && w.cells.indexOf(cell) >= 0) }

    RowLayout {
        anchors.fill: parent
        spacing: 50 * ws.g.s
        Item {
            Layout.fillHeight: true
            Layout.preferredWidth: height
            Grid {
                id: grid
                anchors.centerIn: parent
                columns: ws.n
                spacing: 8 * ws.g.s
                readonly property real cell: (Math.min(parent.width, parent.height) - (ws.n - 1) * spacing) / ws.n
                Repeater {
                    model: ws.letters.length
                    delegate: Rectangle {
                        required property int index
                        readonly property bool found: { ws.words; return ws.isFound(index) }
                        width: grid.cell; height: grid.cell
                        radius: 14 * ws.g.s
                        color: ws.startCell === index ? "#F2B33D" : found ? "#662E8B57" : cellTap.pressed ? ws.g.pressBg : ws.g.cardBg
                        border.color: found ? ws.good : ws.g.lineColor; border.width: found ? 3 : 2
                        Text { anchors.centerIn: parent; text: ws.letters[index]; font.pixelSize: grid.cell * 0.55; font.bold: true; color: ws.g.ink }
                        TapHandler { id: cellTap; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: ws.tap(index) }
                    }
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 18 * ws.g.s
            Text { text: "Thema: " + ws.theme; font.pixelSize: 46 * ws.g.s; font.bold: true; color: ws.g.ink }
            Text {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: "Tik op de eerste letter en dan op de laatste letter van een woord."
                font.pixelSize: 34 * ws.g.s; color: ws.g.soft
            }
            Repeater {
                model: ws.words
                delegate: Text {
                    required property var modelData
                    text: (modelData.found ? "✓  " : "•  ") + modelData.word
                    font.pixelSize: 56 * ws.g.s; font.bold: true
                    font.strikeout: modelData.found
                    color: modelData.found ? ws.good : ws.g.ink
                }
            }
            Item { Layout.fillHeight: true }
        }
    }
}
