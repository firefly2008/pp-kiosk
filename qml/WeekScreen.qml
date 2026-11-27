// Mijn week: vandaag en de 6 dagen erna in één oogopslag (activiteiten, eten, verjaardagen).
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
    property var days: agenda.week()
    Connections { target: agenda; function onChanged() { page.days = agenda.week() } }

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
            TapHandler { id: backTap; onTapped: page.back() }
        }
        Text { text: "🗓️"; font.family: page.emojiFont; font.pixelSize: 56 * page.s }
        Text {
            Layout.fillWidth: true
            text: "Mijn week"
            font.pixelSize: 64 * page.s; font.bold: true; color: page.ink
        }
    }

    // ---------- Zeven dagen ----------
    Row {
        id: cols
        anchors { left: parent.left; right: parent.right; top: top.bottom; bottom: parent.bottom; margins: 40 * page.s; topMargin: 24 * page.s }
        spacing: 16 * page.s
        Repeater {
            model: page.days
            delegate: Rectangle {
                id: day
                required property var modelData
                required property int index
                readonly property bool today: index === 0
                readonly property date d: new Date(modelData.date + "T12:00:00")
                width: (cols.width - 6 * cols.spacing) / 7
                height: cols.height
                radius: 24 * page.s
                color: page.glass ? Qt.rgba(1, 1, 1, today ? page.glassAlpha + 0.08 : page.glassAlpha) : today ? "#FFFFFF" : "#FBFAF7"
                border.color: today ? (page.glass ? "#8BE9C0" : "#2E8B57") : page.lineColor
                border.width: today ? 6 * page.s : 2

                Rectangle {
                    id: head
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: day.border.width }
                    height: 112 * page.s
                    radius: 20 * page.s
                    color: day.today ? (page.glass ? "#992E8B57" : "#2E8B57") : (page.glass ? "#1AFFFFFF" : "#EEF1F5")
                    Column {
                        anchors.centerIn: parent
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: day.today ? "Vandaag" : index === 1 ? "Morgen" : day.d.toLocaleDateString(Qt.locale("nl_NL"), "dddd")
                            font.pixelSize: 36 * page.s; font.bold: true; font.capitalization: Font.Capitalize
                            color: day.today ? "#FFFFFF" : page.ink
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: day.d.toLocaleDateString(Qt.locale("nl_NL"), day.today || index === 1 ? "dddd d MMM" : "d MMMM")
                            font.pixelSize: 26 * page.s
                            color: day.today ? "#E8FFFFFF" : page.soft
                        }
                    }
                }

                ListView {
                    id: list
                    anchors { left: parent.left; right: parent.right; top: head.bottom; bottom: foot.top; margins: 12 * page.s }
                    clip: true
                    spacing: 6 * page.s
                    boundsBehavior: Flickable.StopAtBounds
                    model: day.modelData.items
                    header: Column {
                        width: list.width
                        Repeater {
                            model: day.modelData.feest
                            delegate: Row {
                                required property string modelData
                                readonly property int cut: modelData.indexOf(" ")
                                width: list.width
                                spacing: 8 * page.s
                                bottomPadding: 8 * page.s
                                Text { text: modelData.substring(0, cut); font.family: page.emojiFont; font.pixelSize: 28 * page.s }
                                Text {
                                    width: parent.width - 48 * page.s
                                    text: modelData.substring(cut + 1)
                                    font.pixelSize: 28 * page.s; font.bold: true; color: "#C0392B"
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                    delegate: Rectangle {
                        required property var modelData
                        width: list.width
                        height: 66 * page.s
                        radius: 14 * page.s
                        color: Qt.rgba(0, 0, 0, 0)
                        Rectangle { anchors.fill: parent; radius: parent.radius; color: modelData.color || "#DDE3EA"; opacity: page.glass ? 0.36 : 0.28 }
                        Item {
                            id: ic
                            anchors { left: parent.left; leftMargin: 8 * page.s; verticalCenter: parent.verticalCenter }
                            width: 46 * page.s; height: width
                            Image {
                                anchors.fill: parent
                                visible: !!modelData.imageUrl
                                source: modelData.imageUrl || ""
                                fillMode: Image.PreserveAspectFit
                                sourceSize.width: 128
                                asynchronous: true
                            }
                            Text {
                                anchors.centerIn: parent
                                visible: !modelData.imageUrl
                                text: modelData.icon || ""
                                font.family: page.emojiFont
                                font.pixelSize: 34 * page.s
                            }
                        }
                        Column {
                            id: txt
                            anchors { left: ic.right; right: parent.right; leftMargin: 8 * page.s; rightMargin: 6 * page.s; verticalCenter: parent.verticalCenter }
                            Text { text: modelData.start || ""; font.pixelSize: 22 * page.s; color: page.soft }
                            Text {
                                width: parent.width
                                text: modelData.title || ""
                                font.pixelSize: 26 * page.s; font.bold: true; color: page.ink
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                Rectangle {
                    id: foot
                    visible: day.modelData.dish !== ""
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 12 * page.s }
                    height: visible ? dish.implicitHeight + 20 * page.s : 0
                    radius: 14 * page.s
                    color: page.glass ? "#33FFD970" : "#FFF4CC"
                    Text {
                        id: dishIcon
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 10 * page.s }
                        text: "🍽️"
                        font.family: page.emojiFont
                        font.pixelSize: 28 * page.s
                    }
                    Text {
                        id: dish
                        anchors { left: dishIcon.right; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 8 * page.s; rightMargin: 10 * page.s }
                        text: day.modelData.dish
                        font.pixelSize: 26 * page.s; font.bold: true; color: page.ink
                        wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight
                    }
                }
            }
        }
    }

    Component.onCompleted: console.info("Week screen opened: " + days.length + " days, today " + (days.length ? days[0].items.length : 0) + " items")
}
