// Praatkaart: grote plaatjes om te laten zien wat je wilt of voelt ("Ik wil drinken", "Ik heb pijn").
// Tik: de kaart komt groot in beeld en wordt hardop gezegd; als het in het beheer aan staat,
// krijgt de begeleiding een melding op de beheerpagina (Communicatie > Berichtenbord).
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: page
    color: "#0A0F1E"
    required property real s
    signal back()
    property var shown: null
    readonly property string emojiFont: Qt.platform.os === "windows" ? "Segoe UI Emoji" : "Noto Color Emoji"

    readonly property var cards: [
        { icon: "🥤", text: "Ik wil drinken", tint: "#5DA9E9", alert: false },
        { icon: "🍽️", text: "Ik heb honger", tint: "#F2B33D", alert: false },
        { icon: "🚻", text: "Ik moet naar de wc", tint: "#6CC28B", alert: true },
        { icon: "🤕", text: "Ik heb pijn", tint: "#E5534B", alert: true },
        { icon: "🤒", text: "Ik voel me niet goed", tint: "#E97A6B", alert: true },
        { icon: "😴", text: "Ik ben moe", tint: "#8E7CC3", alert: false },
        { icon: "🥶", text: "Ik heb het koud", tint: "#4FB3C9", alert: false },
        { icon: "🥵", text: "Ik heb het warm", tint: "#FF8A3D", alert: false },
        { icon: "🌳", text: "Ik wil naar buiten", tint: "#3E9A63", alert: false },
        { icon: "🤫", text: "Ik wil even alleen zijn", tint: "#6E6684", alert: false },
        { icon: "🙋", text: "Kun je even komen?", tint: "#D6457A", alert: true },
        { icon: "🆘", text: "Help!", tint: "#C62828", alert: true },
        { icon: "😊", text: "Ik ben blij", tint: "#F6C85F", alert: false },
        { icon: "😢", text: "Ik ben verdrietig", tint: "#5B7DB1", alert: false },
        { icon: "👍", text: "Ja", tint: "#2E8B57", alert: false },
        { icon: "👎", text: "Nee", tint: "#B3261E", alert: false }
    ]
    function choose(c) {
        shown = c
        speaker.say(c.text)
        hideShown.restart()
        if (c.alert && agenda.settings.talkAlert !== false) board.alert(c.icon + " " + c.text)
        stats.count("talk", c.text)
        console.info("Talk card: " + c.text)
    }
    Timer { id: hideShown; interval: 20000; onTriggered: page.shown = null }
    Timer { interval: 5 * 60 * 1000; running: page.shown === null; onTriggered: page.back() }

    GlassBackground { anchors.fill: parent }

    RowLayout {
        id: top
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 40 * page.s }
        height: 110 * page.s
        spacing: 24 * page.s
        Rectangle {
            Layout.preferredWidth: 220 * page.s; Layout.preferredHeight: 100 * page.s
            radius: height / 2
            color: backTap.pressed ? "#40FFFFFF" : "#1FFFFFFF"
            border.color: "#40FFFFFF"; border.width: 2
            Text { anchors.centerIn: parent; text: "‹  Terug"; font.pixelSize: 40 * page.s; font.bold: true; color: "#FFFFFF" }
            TapHandler { id: backTap; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: page.back() }
        }
        Text { Layout.fillWidth: true; text: "Praatkaart"; font.pixelSize: 72 * page.s; font.bold: true; color: "#FFFFFF" }
        Text { text: "Tik op wat je wilt zeggen"; font.pixelSize: 36 * page.s; color: "#C3CAE0" }
    }

    GridLayout {
        anchors { left: parent.left; right: parent.right; top: top.bottom; bottom: parent.bottom; margins: 40 * page.s; topMargin: 24 * page.s }
        columns: 4
        rowSpacing: 20 * page.s; columnSpacing: 20 * page.s
        Repeater {
            model: page.cards
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true; Layout.fillHeight: true
                radius: 32 * page.s
                readonly property color tint: modelData.tint
                color: Qt.rgba(tint.r, tint.g, tint.b, cardTap.pressed ? 0.8 : 0.42)
                border.color: "#55FFFFFF"; border.width: 2
                RowLayout {
                    anchors { fill: parent; leftMargin: 24 * page.s; rightMargin: 20 * page.s }
                    spacing: 18 * page.s
                    Text { text: modelData.icon; font.family: page.emojiFont; font.pixelSize: 80 * page.s }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.text
                        font.pixelSize: 40 * page.s; font.bold: true; color: "#FFFFFF"
                        wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                    }
                }
                TapHandler { id: cardTap; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: page.choose(modelData) }
            }
        }
    }

    // De gekozen kaart groot in beeld (zodat iemand anders hem ook kan zien)
    Rectangle {
        anchors.fill: parent
        visible: page.shown !== null
        color: "#CC000000"
        MouseArea { anchors.fill: parent; onClicked: page.shown = null }
        Rectangle {
            anchors.centerIn: parent
            width: 1200 * page.s; height: 820 * page.s
            radius: 56 * page.s
            readonly property color tint: page.shown ? page.shown.tint : "#000000"
            color: Qt.rgba(tint.r, tint.g, tint.b, 0.92)
            border.color: "#FFFFFF"; border.width: 6 * page.s
            Column {
                anchors.centerIn: parent
                spacing: 24 * page.s
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: page.shown ? page.shown.icon : ""; font.family: page.emojiFont; font.pixelSize: 300 * page.s }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 1100 * page.s; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                    text: page.shown ? page.shown.text : ""
                    font.pixelSize: 100 * page.s; font.bold: true; color: "#FFFFFF"
                    style: Text.Outline; styleColor: "#40000000"
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: !!(page.shown && page.shown.alert && agenda.settings.talkAlert !== false)
                    text: "De begeleiding krijgt een melding."
                    font.pixelSize: 38 * page.s; color: "#F0FFFFFF"
                }
            }
        }
    }
    Component.onCompleted: console.info("Talk screen opened: " + cards.length + " cards")
}
