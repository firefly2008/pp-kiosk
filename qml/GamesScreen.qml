// Spelletjes: memory (kaartjes omdraaien) en puzzel (stukjes van een foto wisselen).
// Groot, rustig en met één vinger te spelen.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: page
    color: glass ? "#0A0F1E" : "#F7F4EE"
    required property real s
    signal back()

    // Glas-thema (gekozen op de beheerpagina): donker met gloed en doorschijnende kaarten
    readonly property bool glass: agenda.settings.theme === "glas" && agenda.settings.contrast !== true
    readonly property real glassAlpha: 1 - Math.max(50, Math.min(95, agenda.settings.glassTransparency || 80)) / 100
    readonly property color cardBg: glass ? Qt.rgba(1, 1, 1, glassAlpha) : "#FFFFFF"
    readonly property color pressBg: glass ? "#40FFFFFF" : "#DDE3EA"
    readonly property color lineColor: glass ? "#40FFFFFF" : "#14000000"
    GlassBackground { anchors.fill: parent; visible: page.glass }

    readonly property color ink: glass ? "#FFFFFF" : "#1F2A44"
    readonly property color soft: glass ? "#C3CAE0" : "#5B6478"
    readonly property string emojiFont: Qt.platform.os === "windows" ? "Segoe UI Emoji" : "Noto Color Emoji"
    property string game: ""          // "", "memory" of "puzzle"
    property bool done: false

    // ---------- Memory ----------
    readonly property var symbols: ["🍎", "🌻", "🐶", "🐱", "🚲", "⚽", "🎈", "🍓", "🐟", "🌈", "☕", "🏠"]
    property var cards: []            // { sym, open, found }
    property int first: -1
    property int second: -1
    property int turns: 0
    function shuffle(a) {
        for (let i = a.length - 1; i > 0; i--) {
            const j = Math.floor(Math.random() * (i + 1));
            [a[i], a[j]] = [a[j], a[i]]
        }
        return a
    }
    function startMemory(pairs) {
        const pick = shuffle(symbols.slice()).slice(0, pairs)
        cards = shuffle(pick.concat(pick)).map(x => ({ sym: x, open: false, found: false }))
        checkTimer.stop()   // een nog lopende controle van het vorige spel mag niet meer afgaan
        first = -1; second = -1; turns = 0; done = false
        game = "memory"
        console.info("Game started: memory " + pairs + " pairs")
        stats.count("game", "Memory " + pairs + " paren")
    }
    function flip(i) {
        if (cards[i].open || cards[i].found || second >= 0) return
        const c = cards.slice()
        c[i] = Object.assign({}, c[i], { open: true })
        cards = c
        if (first < 0) { first = i; return }
        second = i
        turns++
        checkTimer.restart()
    }
    Timer {
        id: checkTimer
        interval: 1100
        onTriggered: {
            if (page.game !== "memory" || page.first < 0 || page.second < 0) return
            const c = page.cards.slice()
            const same = c[page.first].sym === c[page.second].sym
            for (const k of [page.first, page.second])
                c[k] = Object.assign({}, c[k], same ? { found: true } : { open: false })
            page.cards = c
            page.first = -1; page.second = -1
            if (c.every(x => x.found)) { page.done = true; console.info("Game won: memory in " + page.turns + " turns") }
        }
    }

    // ---------- Puzzel ----------
    readonly property var photos: ["pannenkoek", "pizza", "salade", "burger", "soep", "vis", "pasta", "stamppot"]
    property string photo: ""
    property int size: 3
    property var tiles: []            // tiles[plek] = juiste plek van het stukje dat daar ligt
    property int picked: -1
    function startPuzzle(n) {
        size = n
        photo = "qrc:/food/" + photos[Math.floor(Math.random() * photos.length)] + ".jpg"
        let t = []
        for (let i = 0; i < n * n; i++) t.push(i)
        do { t = shuffle(t) } while (t.every((v, i) => v === i))
        tiles = t; picked = -1; turns = 0; done = false
        game = "puzzle"
        console.info("Game started: puzzle " + n + "x" + n)
        stats.count("game", "Puzzel " + (n * n) + " stukjes")
    }
    function pick(i) {
        if (done) return
        if (picked < 0) { picked = i; return }
        if (picked !== i) {
            const t = tiles.slice();
            [t[picked], t[i]] = [t[i], t[picked]]
            tiles = t
            turns++
        }
        picked = -1
        if (tiles.every((v, k) => v === k)) { done = true; console.info("Game won: puzzle in " + turns + " moves") }
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
            color: backTap.pressed ? page.pressBg : page.cardBg
            border.color: page.lineColor; border.width: 2
            Text { anchors.centerIn: parent; text: "‹  Terug"; font.pixelSize: 40 * page.s; font.bold: true; color: page.ink }
            TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds; id: backTap; onTapped: { if (page.game !== "") page.game = ""; else page.back() } }
        }
        Text { text: "🧩"; font.family: page.emojiFont; font.pixelSize: 56 * page.s }
        Text {
            Layout.fillWidth: true
            text: ({ memory: "Memory", puzzle: "Puzzel", quiz: "Quiz van de dag", words: "Woordzoeker", sudoku: "Sudoku", color: "Kleurplaat" })[page.game] || "Spelletjes"
            font.pixelSize: 64 * page.s; font.bold: true; color: page.ink
        }
        Text {
            visible: page.game === "memory" || page.game === "puzzle"
            text: page.game === "memory" ? "Beurten: " + page.turns : "Zetten: " + page.turns
            font.pixelSize: 40 * page.s; color: page.soft
        }
    }

    // ---------- Keuze ----------
    component Choice: Rectangle {
        id: choice
        property string icon
        property string title
        property string sub
        property color tint
        signal chosen()
        radius: 36 * page.s
        color: choiceTap.pressed ? Qt.darker(tint, 1.12) : (page.glass ? Qt.rgba(tint.r, tint.g, tint.b, 0.45) : tint)
        border.color: page.glass ? "#55FFFFFF" : "transparent"; border.width: page.glass ? 2 : 0
        Column {
            anchors.centerIn: parent
            spacing: 12 * page.s
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: choice.icon; font.family: page.emojiFont; font.pixelSize: 96 * page.s }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: choice.title; font.pixelSize: 52 * page.s; font.bold: true; color: "#FFFFFF" }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: choice.sub; font.pixelSize: 32 * page.s; color: "#F0FFFFFF" }
        }
        TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds; id: choiceTap; onTapped: choice.chosen() }
    }
    GridLayout {
        visible: page.game === ""
        anchors { left: parent.left; right: parent.right; top: top.bottom; bottom: parent.bottom; margins: 50 * page.s }
        columns: 4; rowSpacing: 32 * page.s; columnSpacing: 32 * page.s
        Choice { Layout.fillWidth: true; Layout.fillHeight: true; icon: "🃏"; title: "Memory"; sub: "Makkelijk: 6 paren"; tint: "#5DA9E9"; onChosen: page.startMemory(6) }
        Choice { Layout.fillWidth: true; Layout.fillHeight: true; icon: "🃏"; title: "Memory"; sub: "Moeilijker: 10 paren"; tint: "#3F7FC0"; onChosen: page.startMemory(10) }
        Choice { Layout.fillWidth: true; Layout.fillHeight: true; icon: "🧩"; title: "Puzzel"; sub: "Makkelijk: 9 stukjes"; tint: "#6CC28B"; onChosen: page.startPuzzle(3) }
        Choice { Layout.fillWidth: true; Layout.fillHeight: true; icon: "🧩"; title: "Puzzel"; sub: "Moeilijker: 16 stukjes"; tint: "#3E9A63"; onChosen: page.startPuzzle(4) }
        Choice { Layout.fillWidth: true; Layout.fillHeight: true; icon: "❓"; title: "Quiz"; sub: "5 vragen van vandaag"; tint: "#E9A23B"; onChosen: page.startExtra("quiz") }
        Choice { Layout.fillWidth: true; Layout.fillHeight: true; icon: "🔎"; title: "Woordzoeker"; sub: "Vind 5 woorden"; tint: "#D6457A"; onChosen: page.startExtra("words") }
        Choice { Layout.fillWidth: true; Layout.fillHeight: true; icon: "🔢"; title: "Sudoku"; sub: "Makkelijk: 4 bij 4"; tint: "#8E7CC3"; onChosen: page.startExtra("sudoku") }
        Choice { Layout.fillWidth: true; Layout.fillHeight: true; icon: "🖍️"; title: "Kleurplaat"; sub: "Rustig kleuren"; tint: "#4FB3C9"; onChosen: page.startExtra("color") }
    }

    // ---------- Memory-bord ----------
    Item {
        id: board
        visible: page.game === "memory"
        anchors { left: parent.left; right: parent.right; top: top.bottom; bottom: parent.bottom; margins: 40 * page.s; topMargin: 24 * page.s }
        readonly property int cols: page.cards.length > 12 ? 5 : 4
        readonly property int rows: Math.ceil(page.cards.length / cols)
        readonly property real cell: Math.min(width / cols, height / rows)
        Grid {
            anchors.centerIn: parent
            columns: board.cols
            Repeater {
                model: page.cards.length
                delegate: Item {
                    required property int index
                    readonly property var card: page.cards[index] || ({})
                    width: board.cell; height: board.cell
                    Rectangle {
                        anchors { fill: parent; margins: 10 * page.s }
                        radius: 28 * page.s
                        color: card.found ? (page.glass ? "#4D6CC28B" : "#DFF3E6") : card.open ? (page.glass ? "#E6FFFFFF" : "#FFFFFF") : (cardTap.pressed ? "#3F7FC0" : (page.glass ? "#995DA9E9" : "#5DA9E9"))
                        border.color: card.found ? "#6CC28B" : "#14000000"; border.width: card.found ? 6 * page.s : 2
                        Text {
                            anchors.centerIn: parent
                            visible: card.open || card.found
                            text: card.sym || ""
                            font.family: page.emojiFont
                            font.pixelSize: parent.height * 0.55
                        }
                        Text {
                            anchors.centerIn: parent
                            visible: !(card.open || card.found)
                            text: "?"
                            font.pixelSize: parent.height * 0.4; font.bold: true; color: "#FFFFFF"
                        }
                        TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds; id: cardTap; onTapped: page.flip(index) }
                    }
                }
            }
        }
    }

    // ---------- Puzzelbord ----------
    Item {
        id: puzzle
        visible: page.game === "puzzle"
        anchors { left: parent.left; right: parent.right; top: top.bottom; bottom: parent.bottom; margins: 40 * page.s; topMargin: 24 * page.s }
        readonly property real boardSize: Math.min(height, width * 0.6)
        readonly property real cell: boardSize / page.size
        // Voorbeeld
        Column {
            anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 20 * page.s }
            spacing: 16 * page.s
            Text { text: "Zo moet het worden:"; font.pixelSize: 36 * page.s; color: page.soft }
            Image { width: 420 * page.s; height: width; source: page.photo; fillMode: Image.PreserveAspectCrop }
            Text { width: 420 * page.s; wrapMode: Text.WordWrap; text: "Tik op een stukje en daarna op de plek waar het hoort."; font.pixelSize: 32 * page.s; color: page.soft }
        }
        Grid {
            x: (parent.width - 460 * page.s - puzzle.boardSize) / 2
            anchors.verticalCenter: parent.verticalCenter
            columns: page.size
            Repeater {
                model: page.tiles.length
                delegate: Item {
                    required property int index
                    readonly property int piece: page.tiles[index] !== undefined ? page.tiles[index] : index
                    width: puzzle.cell; height: puzzle.cell
                    Item {
                        anchors { fill: parent; margins: page.done ? 0 : 3 * page.s }
                        clip: true
                        Image {
                            width: puzzle.boardSize; height: puzzle.boardSize
                            x: -(piece % page.size) * puzzle.cell
                            y: -Math.floor(piece / page.size) * puzzle.cell
                            source: page.photo
                            fillMode: Image.PreserveAspectCrop
                            sourceSize.width: 768
                        }
                    }
                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        border.color: page.picked === index ? "#F2B33D" : "transparent"
                        border.width: 10 * page.s
                    }
                    TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: page.pick(index) }
                }
            }
        }
    }

    // ---------- Quiz, woordzoeker, sudoku en kleurplaat (elk een eigen bestand) ----------
    function startExtra(k) {
        done = false
        stats.count("game", ({ quiz: "Quiz", words: "Woordzoeker", sudoku: "Sudoku", color: "Kleurplaat" })[k])
        if (game === k && extra.item) extra.item.start()   // nog een keer
        else game = k                                      // laden start het spel
    }
    Loader {
        id: extra
        anchors { left: parent.left; right: parent.right; top: top.bottom; bottom: parent.bottom; margins: 40 * page.s; topMargin: 24 * page.s }
        active: ["quiz", "words", "sudoku", "color"].indexOf(page.game) >= 0
        sourceComponent: page.game === "quiz" ? quizC : page.game === "words" ? wordsC : page.game === "sudoku" ? sudokuC : page.game === "color" ? colorC : null
        onLoaded: item.start()
    }
    Component { id: quizC; QuizGame { g: page; onWon: page.done = true } }
    Component { id: wordsC; WordSearch { g: page; onWon: page.done = true } }
    Component { id: sudokuC; SudokuGame { g: page; onWon: page.done = true } }
    Component { id: colorC; ColorGame { g: page; onWon: page.done = true } }

    // ---------- Gewonnen ----------
    Rectangle {
        anchors.centerIn: parent
        visible: page.done
        width: 900 * page.s; height: 420 * page.s
        radius: 40 * page.s
        color: page.glass ? "#E61A2036" : "#FFFFFF"
        border.color: "#6CC28B"; border.width: 8 * page.s
        Column {
            anchors.centerIn: parent
            spacing: 24 * page.s
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "🎉  Goed gedaan!"; font.family: page.emojiFont; font.pixelSize: 80 * page.s; font.bold: true; color: page.ink }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 30 * page.s
                Rectangle {
                    width: 380 * page.s; height: 110 * page.s; radius: 28 * page.s
                    color: againTap.pressed ? "#3E9A63" : "#6CC28B"
                    Text { anchors.centerIn: parent; text: "Nog een keer"; font.pixelSize: 44 * page.s; font.bold: true; color: "#FFFFFF" }
                    TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds; id: againTap; onTapped: page.game === "memory" ? page.startMemory(page.cards.length / 2) : page.game === "puzzle" ? page.startPuzzle(page.size) : page.startExtra(page.game) }
                }
                Rectangle {
                    width: 320 * page.s; height: 110 * page.s; radius: 28 * page.s
                    color: stopTap.pressed ? page.pressBg : (page.glass ? "#26FFFFFF" : "#EEF1F5")
                    Text { anchors.centerIn: parent; text: "Stoppen"; font.pixelSize: 44 * page.s; font.bold: true; color: page.ink }
                    TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds; id: stopTap; onTapped: { page.done = false; page.game = "" } }
                }
            }
        }
    }

    Component.onCompleted: console.info("Games screen opened")
}
