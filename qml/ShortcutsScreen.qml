// Snelkoppelingen: grote tegels naar Nederlandse mediaproviders en eigen websites (per groep).
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: page
    color: "#F7F4EE"
    required property real s
    signal back()

    readonly property color ink: "#1F2A44"
    readonly property string emojiFont: Qt.platform.os === "windows" ? "Segoe UI Emoji" : "Noto Color Emoji"

    // Groepen in volgorde van de lijst: [{ name, items: [...] }]
    readonly property var groups: {
        const out = []
        for (const it of shortcuts.list) {
            let g = out.find(x => x.name === it.group)
            if (!g) { g = { name: it.group, items: [] }; out.push(g) }
            g.items.push(it)
        }
        return out
    }

    function open(item) {
        appModel.launchWeb(item.url)
        homeBridge.externalLaunched()
        console.info("Shortcut opened: " + item.name)
    }

    RowLayout {
        id: top
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 40 * page.s }
        height: 120 * page.s
        spacing: 24 * page.s
        Rectangle {
            Layout.preferredWidth: 220 * page.s; Layout.preferredHeight: 100 * page.s
            radius: 24 * page.s
            color: backTap.pressed ? "#DDE3EA" : "#FFFFFF"
            border.color: "#14000000"; border.width: 2
            Text { anchors.centerIn: parent; text: "‹  Terug"; font.pixelSize: 40 * page.s; font.bold: true; color: page.ink }
            TapHandler { id: backTap; onTapped: page.back() }
        }
        Text { text: "⭐"; font.family: page.emojiFont; font.pixelSize: 56 * page.s }
        Text {
            Layout.fillWidth: true
            text: "Snelkoppelingen"
            font.pixelSize: 64 * page.s; font.bold: true; color: page.ink
        }
    }

    Flickable {
        id: flick
        anchors { left: parent.left; right: parent.right; top: top.bottom; bottom: parent.bottom; margins: 40 * page.s; topMargin: 16 * page.s }
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentHeight: col.implicitHeight
        Column {
            id: col
            width: flick.width
            spacing: 24 * page.s
            Repeater {
                model: page.groups
                delegate: Column {
                    id: group
                    required property var modelData
                    width: col.width
                    spacing: 12 * page.s
                    Text {
                        text: group.modelData.name
                        font.pixelSize: 44 * page.s; font.bold: true; color: "#5B6478"
                        leftPadding: 12 * page.s
                    }
                    Flow {
                        width: parent.width
                        Repeater {
                            model: group.modelData.items
                            delegate: Item {
                                required property var modelData
                                width: col.width / 5; height: 210 * page.s
                                Rectangle {
                                    anchors { fill: parent; margins: 12 * page.s }
                                    radius: 28 * page.s
                                    color: tileTap.pressed ? Qt.darker(String(modelData.color || "#3A4A6B"), 1.2) : String(modelData.color || "#3A4A6B")
                                    Text {
                                        anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 18 * page.s }
                                        text: modelData.icon || "🌐"
                                        font.family: page.emojiFont
                                        font.pixelSize: 64 * page.s
                                    }
                                    Text {
                                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 16 * page.s }
                                        horizontalAlignment: Text.AlignHCenter
                                        text: modelData.name
                                        font.pixelSize: 36 * page.s; font.bold: true; color: "#FFFFFF"
                                        elide: Text.ElideRight
                                        style: Text.Raised; styleColor: "#40000000"
                                    }
                                    TapHandler { id: tileTap; onTapped: page.open(modelData) }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: shortcuts.list.length === 0
        text: "Nog geen snelkoppelingen. Voeg ze toe via de beheerpagina."
        font.pixelSize: 40 * page.s; color: "#5B6478"
    }

    Component.onCompleted: console.info("Shortcuts screen opened: " + shortcuts.list.length)
}
