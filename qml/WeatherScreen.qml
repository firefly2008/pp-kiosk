// Weer: het weer van nu groot, met vandaag, morgen en overmorgen. Altijd in de glasstijl.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: page
    color: "#0A0F1E"
    required property real s
    signal back()
    readonly property color glassCard: "#1FFFFFFF"
    readonly property color glassLine: "#40FFFFFF"
    readonly property string emojiFont: Qt.platform.os === "windows" ? "Segoe UI Emoji" : "Noto Color Emoji"

    Timer { interval: 3 * 60 * 1000; running: true; onTriggered: page.back() }
    GlassBackground { anchors.fill: parent }

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
        Text {
            Layout.fillWidth: true
            text: "Het weer" + (weather.place ? " in " + weather.place : "")
            font.pixelSize: 72 * page.s; font.bold: true; color: "#FFFFFF"
            elide: Text.ElideRight
        }
    }

    Text {
        anchors.centerIn: parent
        visible: !weather.available
        text: "Het weer is nu niet bekend. Is er internet?"
        font.pixelSize: 48 * page.s; color: "#C3CAE0"
    }

    ColumnLayout {
        visible: weather.available
        anchors { left: parent.left; right: parent.right; top: top.bottom; bottom: parent.bottom; margins: 40 * page.s; topMargin: 24 * page.s }
        spacing: 32 * page.s
        // Nu
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 330 * page.s
            radius: 40 * page.s
            color: page.glassCard; border.color: page.glassLine; border.width: 2
            RowLayout {
                anchors.centerIn: parent
                spacing: 60 * page.s
                Text { text: weather.icon; font.family: page.emojiFont; font.pixelSize: 220 * page.s }
                Column {
                    Text { text: "NU"; font.pixelSize: 40 * page.s; font.bold: true; color: "#8BE9C0" }
                    Text { text: weather.temperature + "°"; font.pixelSize: 170 * page.s; font.bold: true; color: "#FFFFFF" }
                    Text { text: weather.text; font.pixelSize: 54 * page.s; color: "#E6EAF5" }
                }
            }
        }
        // Drie dagen
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 32 * page.s
            Repeater {
                model: weather.days
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 36 * page.s
                    color: page.glassCard; border.color: page.glassLine; border.width: 2
                    Column {
                        anchors.centerIn: parent
                        spacing: 8 * page.s
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.day; font.pixelSize: 54 * page.s; font.bold: true; color: "#FFFFFF"; font.capitalization: Font.Capitalize }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.icon; font.family: page.emojiFont; font.pixelSize: 120 * page.s }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.text; font.pixelSize: 40 * page.s; color: "#E6EAF5" }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.max + "°  /  " + modelData.min + "°"
                            font.pixelSize: 56 * page.s; font.bold: true; color: "#FFFFFF"
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "☂ kans op regen " + modelData.rain + "%"
                            font.pixelSize: 34 * page.s; color: modelData.rain >= 50 ? "#8FC8FF" : "#C3CAE0"
                        }
                    }
                }
            }
        }
    }
    Component.onCompleted: console.info("Weather page opened: " + (weather.available ? "yes" : "no") + ", days " + weather.days.length)
}
