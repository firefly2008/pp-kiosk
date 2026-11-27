// Seizoensthema: een paar rustig vallende of zwevende figuurtjes, passend bij de tijd van het jaar.
// Bewust weinig en langzaam, zodat het niet afleidt. Tikken gaat er dwars doorheen.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

Item {
    id: decor
    property string theme: "herfst"     // lente, zomer, herfst, winter, sinterklaas, kerst
    readonly property var icons: ({
        lente: ["🌸", "🌷", "🦋"],
        zomer: ["☀️", "🌻", "🦋"],
        herfst: ["🍂", "🍁", "🌰"],
        winter: ["❄️", "❄️", "⛄"],
        sinterklaas: ["🎁", "🍪", "⭐"],
        kerst: ["❄️", "⭐", "🎄"]
    })[theme] || ["🍂"]
    readonly property string emojiFont: Qt.platform.os === "windows" ? "Segoe UI Emoji" : "Noto Color Emoji"
    readonly property real s: Math.min(width / 1920, height / 1080)
    enabled: false   // geen aanraking opvangen

    Repeater {
        model: decor.visible ? 9 : 0
        delegate: Text {
            id: leaf
            required property int index
            text: decor.icons[index % decor.icons.length]
            font.family: decor.emojiFont
            font.pixelSize: (34 + (index % 3) * 12) * decor.s
            opacity: 0.55
            x: ((index * 211) % 100) / 100 * decor.width
            NumberAnimation on y {
                loops: Animation.Infinite
                from: -80 * decor.s; to: decor.height + 40 * decor.s
                duration: 22000 + (leaf.index % 5) * 5000
            }
            SequentialAnimation on rotation {
                loops: Animation.Infinite
                NumberAnimation { from: -20; to: 20; duration: 3000 + leaf.index * 300; easing.type: Easing.InOutSine }
                NumberAnimation { from: 20; to: -20; duration: 3000 + leaf.index * 300; easing.type: Easing.InOutSine }
            }
            Component.onCompleted: y = -80 * decor.s - (leaf.index * 137 % 600) * decor.s
        }
    }
}
