// Onderweg: vertrektijden van bus, tram, metro en trein dichtbij, en een knop naar de verkeersinformatie.
// Altijd in de glasstijl. Elke minuut ververst.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: page
    color: "#0A0F1E"
    required property real s
    required property var home
    signal back()
    readonly property string emojiFont: Qt.platform.os === "windows" ? "Segoe UI Emoji" : "Noto Color Emoji"
    readonly property var icons: ({ bus: "🚌", tram: "🚋", metro: "🚇", trein: "🚆", boot: "⛴️" })

    Timer { interval: 60000; running: true; repeat: true; triggeredOnStart: true; onTriggered: transit.refresh() }
    Timer { interval: 5 * 60 * 1000; running: true; onTriggered: page.back() }

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
        Column {
            Layout.fillWidth: true
            Text { text: "Onderweg"; font.pixelSize: 64 * page.s; font.bold: true; color: "#FFFFFF" }
            Text { width: parent.width; text: transit.title ? "Vertrek vanaf " + transit.title : ""; font.pixelSize: 32 * page.s; color: "#C3CAE0"; elide: Text.ElideRight }
        }
        Rectangle {
            Layout.preferredWidth: trafficText.implicitWidth + 80 * page.s; Layout.preferredHeight: 100 * page.s
            radius: height / 2
            color: trafficTap.pressed ? "#40FFFFFF" : "#1FFFFFFF"
            border.color: "#40FFFFFF"; border.width: 2
            Text { id: trafficText; anchors.centerIn: parent; text: "🚗  Verkeer en files"; font.pixelSize: 38 * page.s; font.bold: true; color: "#FFFFFF" }
            TapHandler {
                id: trafficTap
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onTapped: {
                    page.home.stopRadio()
                    appModel.launchWeb(agenda.settings.trafficUrl || "https://www.anwb.nl/verkeer/nederland/verkeersinformatie")
                    homeBridge.externalLaunched()
                    stats.count("tile", "Verkeer")
                }
            }
        }
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; top: top.bottom; bottom: parent.bottom; margins: 40 * page.s; topMargin: 24 * page.s }
        radius: 36 * page.s
        color: "#1FFFFFFF"; border.color: "#40FFFFFF"; border.width: 2
        clip: true

        Text {
            anchors.centerIn: parent
            visible: transit.departures.length === 0
            width: parent.width * 0.8
            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            text: transit.status || "Vertrektijden ophalen…"
            font.pixelSize: 44 * page.s; color: "#C3CAE0"
        }
        Column {
            anchors { fill: parent; margins: 30 * page.s }
            spacing: 10 * page.s
            visible: transit.departures.length > 0
            // Kopregel
            RowLayout {
                width: parent.width
                spacing: 24 * page.s
                Text { Layout.preferredWidth: 170 * page.s; text: "Tijd"; font.pixelSize: 30 * page.s; color: "#C3CAE0" }
                Text { Layout.preferredWidth: 220 * page.s; text: "Lijn"; font.pixelSize: 30 * page.s; color: "#C3CAE0" }
                Text { Layout.fillWidth: true; text: "Naar"; font.pixelSize: 30 * page.s; color: "#C3CAE0" }
                Text { Layout.preferredWidth: 200 * page.s; text: "Spoor"; font.pixelSize: 30 * page.s; color: "#C3CAE0"; visible: transit.departures.some(d => !!d.track) }
            }
            Repeater {
                model: transit.departures
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: parent.width
                    height: 88 * page.s
                    radius: 22 * page.s
                    color: index === 0 ? "#332E8B57" : "#14FFFFFF"
                    RowLayout {
                        anchors { fill: parent; leftMargin: 10 * page.s; rightMargin: 20 * page.s }
                        spacing: 24 * page.s
                        Row {
                            Layout.preferredWidth: 170 * page.s
                            spacing: 8 * page.s
                            Text { text: modelData.time; font.pixelSize: 46 * page.s; font.bold: true; color: "#FFFFFF"; font.strikeout: !!modelData.cancelled }
                            Text { visible: modelData.delay > 0; anchors.bottom: parent.bottom; text: "+" + modelData.delay; font.pixelSize: 32 * page.s; font.bold: true; color: "#FF8A80" }
                        }
                        Row {
                            Layout.preferredWidth: 220 * page.s
                            spacing: 12 * page.s
                            Text { text: page.icons[modelData.kind] || "🚌"; font.family: page.emojiFont; font.pixelSize: 44 * page.s }
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.max(80 * page.s, lineText.implicitWidth + 24 * page.s); height: 60 * page.s; radius: 12 * page.s
                                color: modelData.kind === "trein" ? "#FFC917" : "#5DA9E9"
                                Text { id: lineText; anchors.centerIn: parent; text: modelData.line; font.pixelSize: 36 * page.s; font.bold: true; color: modelData.kind === "trein" ? "#003082" : "#FFFFFF" }
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.cancelled ? modelData.to + "  (rijdt niet)" : modelData.to
                            font.pixelSize: 44 * page.s; font.bold: index === 0; color: modelData.cancelled ? "#FF8A80" : "#FFFFFF"
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.preferredWidth: 200 * page.s
                            visible: transit.departures.some(d => !!d.track)
                            text: modelData.track ? "spoor " + modelData.track : ""
                            font.pixelSize: 38 * page.s; color: "#E6EAF5"
                        }
                    }
                }
            }
        }
        Text {
            anchors { right: parent.right; bottom: parent.bottom; margins: 20 * page.s }
            visible: transit.departures.length > 0 && transit.status !== ""
            text: transit.status
            font.pixelSize: 26 * page.s; color: "#FF8A80"
        }
    }
    Component.onCompleted: console.info("Travel screen opened")
}
