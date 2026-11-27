// Kleurplaat: kies een kleur en tik (of veeg) over de vakjes. Een lichte kleur laat zien waar welke kleur hoort.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts

Item {
    id: cg
    required property var g
    signal won()
    readonly property int n: 9
    readonly property var palette: ({ R: "#E5534B", G: "#3E9A63", Y: "#F6C85F", B: "#3F7FC0", O: "#FF8A3D", K: "#8B5A2B", P: "#E58AC3", W: "#F4F4F4" })
    readonly property var pictures: [
        { name: "Hart", rows: [".RR...RR.", "RRRR.RRRR", "RRRRRRRRR", "RRRRRRRRR", ".RRRRRRR.", "..RRRRR..", "...RRR...", "....R....", "........."] },
        { name: "Bloem", rows: ["...PPP...", "..PPPPP..", "..PPYPP..", "..PPPPP..", "...PPP...", "....G....", ".GG.G....", "..GGG.GG.", "....GGG.."] },
        { name: "Zon", rows: ["Y...Y...Y", ".Y..Y..Y.", "..YYYYY..", "..YOOOY..", "YYYOOOYYY", "..YOOOY..", "..YYYYY..", ".Y..Y..Y.", "Y...Y...Y"] },
        { name: "Huis", rows: ["....R....", "...RRR...", "..RRRRR..", ".RRRRRRR.", ".YYYYYYY.", ".YBBYKKY.", ".YBBYKKY.", ".YYYYKKY.", "GGGGGGGGG"] },
        { name: "Vis", rows: [".........", "...BBB...", ".BBBBBB.O", "BWBBBBBOO", "BBBBBBBBO", ".BBBBBB.O", "...BBB...", ".........", "........."] },
        { name: "Boom", rows: ["...GGG...", "..GGGGG..", ".GGGGGGG.", ".GGRGGGG.", "..GGGGRG.", "...GGG...", "....K....", "....K....", "GGGGKGGGG"] }
    ]
    property var pic: pictures[0]
    property var paint: []          // per vakje de gekozen letter ("" = nog leeg)
    property string brush: "R"
    readonly property var used: { const u = []; for (const r of pic.rows) for (const ch of r) if (ch !== "." && u.indexOf(ch) < 0) u.push(ch); return u }

    function start() {
        pic = pictures[Math.floor(Math.random() * pictures.length)]
        paint = new Array(n * n).fill("")
        brush = used[0]
        console.info("Game started: coloring " + pic.name)
    }
    function target(i) { return pic.rows[Math.floor(i / n)][i % n] }
    function color(i) {
        if (i < 0 || i >= n * n || target(i) === ".") return
        if (paint[i] === brush) return
        const p = paint.slice(); p[i] = brush; paint = p
        let doneAll = true
        for (let k = 0; k < n * n; ++k) if (target(k) !== "." && paint[k] !== target(k)) { doneAll = false; break }
        if (doneAll) { console.info("Game won: coloring " + pic.name); cg.won() }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 50 * cg.g.s
        Item {
            Layout.fillHeight: true
            Layout.preferredWidth: height
            Grid {
                id: canvas
                anchors.centerIn: parent
                columns: cg.n
                spacing: 4 * cg.g.s
                readonly property real cell: (Math.min(parent.width, parent.height) - (cg.n - 1) * spacing) / cg.n
                Repeater {
                    model: cg.n * cg.n
                    delegate: Rectangle {
                        required property int index
                        readonly property string want: cg.target(index)
                        readonly property string have: cg.paint[index] || ""
                        width: canvas.cell; height: canvas.cell
                        radius: 10 * cg.g.s
                        color: want === "." ? "transparent" : have !== "" ? cg.palette[have] : Qt.rgba(Qt.color(cg.palette[want]).r, Qt.color(cg.palette[want]).g, Qt.color(cg.palette[want]).b, 0.22)
                        border.color: want === "." ? "transparent" : cg.g.lineColor; border.width: 2
                    }
                }
            }
            // Tikken en vegen kleurt de vakjes onder de vinger
            MouseArea {
                anchors.fill: canvas
                function at(x, y) {
                    const step = canvas.cell + canvas.spacing
                    const c = Math.floor(x / step), r = Math.floor(y / step)
                    if (c >= 0 && c < cg.n && r >= 0 && r < cg.n) cg.color(r * cg.n + c)
                }
                onPressed: mouse => at(mouse.x, mouse.y)
                onPositionChanged: mouse => at(mouse.x, mouse.y)
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 20 * cg.g.s
            Text { text: "Kleurplaat: " + cg.pic.name; font.pixelSize: 50 * cg.g.s; font.bold: true; color: cg.g.ink }
            Text {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: "Kies een kleur. Tik of veeg over de lichte vakjes van dezelfde kleur."
                font.pixelSize: 34 * cg.g.s; color: cg.g.soft
            }
            Flow {
                Layout.fillWidth: true
                spacing: 24 * cg.g.s
                Repeater {
                    model: cg.used
                    delegate: Rectangle {
                        required property string modelData
                        width: 150 * cg.g.s; height: width
                        radius: width / 2
                        color: cg.palette[modelData]
                        border.color: cg.brush === modelData ? cg.g.ink : "#55FFFFFF"
                        border.width: cg.brush === modelData ? 10 * cg.g.s : 3
                        scale: cg.brush === modelData ? 1.08 : 1
                        TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: cg.brush = modelData }
                    }
                }
            }
            Item { Layout.fillHeight: true }
        }
    }
}
