// Nieuws: de berichten van NOS rustig lezen, één bericht per keer, alles op één scherm (geen scrollen).
// Altijd in de glasstijl (donker met zachte gloed en doorschijnende kaarten), welk thema er ook gekozen is.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: page
    color: "#0A0F1E"
    required property real s
    property int current: 0
    signal back()

    readonly property var items: news.headlines
    readonly property var item: items.length > 0 ? items[Math.min(current, items.length - 1)] : null
    readonly property color glassCard: "#1FFFFFFF"
    readonly property color glassLine: "#40FFFFFF"
    readonly property string emojiFont: Qt.platform.os === "windows" ? "Segoe UI Emoji" : "Noto Color Emoji"

    function go(d) {
        if (items.length === 0) return
        current = (current + d + items.length) % items.length
        hideTimer.restart()
    }
    // Na 3 minuten zonder aanraken terug naar het startscherm
    Timer { id: hideTimer; interval: 3 * 60 * 1000; running: true; onTriggered: page.back() }

    component Pill: Rectangle {
        property string label
        signal hit()
        Layout.preferredHeight: 100 * page.s
        Layout.preferredWidth: 300 * page.s
        radius: height / 2
        color: pt.pressed ? "#55FFFFFF" : "#26FFFFFF"
        border.color: page.glassLine; border.width: 2
        Text { anchors.centerIn: parent; text: parent.label; font.pixelSize: 40 * page.s; font.bold: true; color: "#FFFFFF" }
        TapHandler { id: pt; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: parent.hit() }
    }

    GlassBackground { anchors.fill: parent }

    // ---------- Kop ----------
    RowLayout {
        id: top
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 40 * page.s }
        height: 110 * page.s
        spacing: 24 * page.s
        Rectangle {
            Layout.preferredWidth: 220 * page.s; Layout.preferredHeight: 100 * page.s
            radius: height / 2
            color: backTap.pressed ? "#40FFFFFF" : page.glassCard
            border.color: page.glassLine; border.width: 2
            Text { anchors.centerIn: parent; text: "‹  Terug"; font.pixelSize: 40 * page.s; font.bold: true; color: "#FFFFFF" }
            TapHandler { id: backTap; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: page.back() }
        }
        Rectangle {
            Layout.preferredHeight: 72 * page.s
            Layout.preferredWidth: nosLabel.implicitWidth + 40 * page.s
            radius: 18 * page.s
            color: "#E1251B"
            Text { id: nosLabel; anchors.centerIn: parent; text: "NOS"; font.pixelSize: 40 * page.s; font.bold: true; font.letterSpacing: 3 * page.s; color: "#FFFFFF" }
        }
        Text {
            Layout.fillWidth: true
            text: "Nieuws"
            font.pixelSize: 72 * page.s; font.bold: true; color: "#FFFFFF"; font.letterSpacing: -1 * page.s
        }
        Text {
            visible: page.items.length > 0
            text: (page.current + 1) + " van " + page.items.length
            font.pixelSize: 36 * page.s; color: "#C3CAE0"
        }
    }

    // ---------- Het bericht ----------
    Rectangle {
        id: article
        anchors { left: parent.left; top: top.bottom; bottom: parent.bottom; margins: 40 * page.s; topMargin: 24 * page.s }
        width: (parent.width - 120 * page.s) * 0.64
        radius: 40 * page.s
        color: page.glassCard
        border.color: page.glassLine; border.width: 2
        clip: true

        Text {
            anchors.centerIn: parent
            visible: page.item === null
            width: parent.width - 120 * page.s
            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            text: "Er is nu geen nieuws. Is er internet?"
            font.pixelSize: 44 * page.s; color: "#C3CAE0"
        }

        ColumnLayout {
            visible: page.item !== null
            anchors { fill: parent; margins: 44 * page.s }
            spacing: 18 * page.s
            RowLayout {
                Layout.fillWidth: true
                spacing: 36 * page.s
                Image {
                    id: photo
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: visible ? 440 * page.s : 0
                    Layout.preferredHeight: 300 * page.s
                    visible: !!(page.item && page.item.image) && status !== Image.Error
                    source: page.item && page.item.image ? page.item.image : ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize.width: 900
                    layer.enabled: false
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 16 * page.s
                    Text {
                        Layout.fillWidth: true
                        text: page.item ? page.item.title : ""
                        font.pixelSize: 56 * page.s; font.bold: true; color: "#FFFFFF"
                        wrapMode: Text.WordWrap; maximumLineCount: 4; elide: Text.ElideRight
                        lineHeight: 1.05
                    }
                    Text {
                        visible: !!(page.item && page.item.time)
                        text: page.item ? "Om " + page.item.time + " uur" : ""
                        font.pixelSize: 32 * page.s; color: "#8BE9C0"; font.bold: true
                    }
                }
            }
            // De tekst van het bericht: zo groot als past, met … als het niet helemaal past
            Text {
                id: bodyBelow
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !!(page.item && page.item.text)
                text: page.item ? page.item.text : ""
                font.pixelSize: 36 * page.s; color: "#E6EAF5"
                lineHeight: 1.2
                wrapMode: Text.WordWrap; elide: Text.ElideRight
                clip: true
            }
            Item { Layout.fillHeight: true; visible: !bodyBelow.visible }
            // Vorige / volgende
            RowLayout {
                Layout.fillWidth: true
                spacing: 20 * page.s
                Pill { label: "‹  Vorige"; onHit: page.go(-1) }
                Item { Layout.fillWidth: true }
                Pill {
                    visible: agenda.settings.readAloud !== false
                    label: "🔊  Voorlezen"
                    onHit: { if (page.item) speaker.say(page.item.title + ". " + (page.item.text || "")); hideTimer.restart() }
                }
                Item { Layout.fillWidth: true }
                Pill { label: "Volgende  ›"; onHit: page.go(1) }
            }
        }
    }

    // ---------- Alle koppen: tik om te lezen ----------
    Column {
        id: list
        anchors { left: article.right; right: parent.right; top: article.top; bottom: article.bottom; leftMargin: 40 * page.s; rightMargin: 40 * page.s }
        spacing: 12 * page.s
        readonly property int rows: 8
        readonly property real rowHeight: (height - (rows - 1) * spacing) / rows
        Repeater {
            model: Math.min(list.rows, page.items.length)
            delegate: Rectangle {
                required property int index
                // De lijst schuift mee, zodat het gekozen bericht altijd in beeld is
                readonly property int first: Math.max(0, Math.min(page.current - 2, page.items.length - list.rows))
                readonly property int idx: first + index
                readonly property bool on: idx === page.current
                width: list.width; height: list.rowHeight
                radius: 24 * page.s
                color: on ? "#662E8B57" : rowTap.pressed ? "#40FFFFFF" : page.glassCard
                border.color: on ? "#8BE9C0" : page.glassLine; border.width: on ? 3 : 2
                RowLayout {
                    anchors { fill: parent; leftMargin: 22 * page.s; rightMargin: 22 * page.s }
                    spacing: 16 * page.s
                    Text {
                        text: page.items[idx] ? page.items[idx].time : ""
                        font.pixelSize: 26 * page.s; color: "#C3CAE0"
                    }
                    Text {
                        Layout.fillWidth: true
                        text: page.items[idx] ? page.items[idx].title : ""
                        font.pixelSize: 28 * page.s; font.bold: on; color: "#FFFFFF"
                        wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                    }
                }
                TapHandler { id: rowTap; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: { page.current = idx; hideTimer.restart() } }
            }
        }
    }

    // Vegen naar links/rechts over het bericht = volgende/vorige
    DragHandler {
        target: null
        xAxis.enabled: true; yAxis.enabled: false
        onActiveChanged: if (!active && Math.abs(centroid.position.x - centroid.pressPosition.x) > 160 * page.s)
                             page.go(centroid.position.x < centroid.pressPosition.x ? 1 : -1)
    }

    Component.onCompleted: console.info("News page opened: " + items.length + " items, text " + (item && item.text ? "yes" : "no"))
}
