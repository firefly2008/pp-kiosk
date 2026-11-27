// Grote analoge klok in glasstijl: doorschijnende wijzerplaat, zachte rand, duidelijke wijzers.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

Item {
    id: clock
    property date now: new Date()
    property color ink: "#FFFFFF"
    property color accent: "#8BE9C0"
    property bool glass: true
    implicitWidth: 200; implicitHeight: 200

    readonly property real r: Math.min(width, height) / 2
    readonly property real hours: now.getHours() % 12 + now.getMinutes() / 60
    readonly property real minutes: now.getMinutes() + now.getSeconds() / 60

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: clock.glass ? "#26FFFFFF" : "#FFFFFF"
        border.color: clock.glass ? "#66FFFFFF" : "#2E8B57"; border.width: Math.max(2, clock.r * 0.04)
        // Glans bovenaan
        Rectangle {
            visible: clock.glass
            width: parent.width * 0.7; height: parent.height * 0.35
            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: parent.height * 0.06 }
            radius: height / 2
            gradient: Gradient {
                GradientStop { position: 0; color: "#33FFFFFF" }
                GradientStop { position: 1; color: "#00FFFFFF" }
            }
        }
    }
    // Streepjes voor de uren (dik bij 12, 3, 6, 9)
    Repeater {
        model: 12
        delegate: Rectangle {
            required property int index
            readonly property bool major: index % 3 === 0
            width: clock.r * (major ? 0.07 : 0.035); height: clock.r * (major ? 0.18 : 0.11)
            radius: width / 2
            color: major ? clock.accent : clock.ink
            opacity: major ? 1 : 0.7
            x: clock.r - width / 2; y: clock.r * 0.08
            transform: Rotation { origin.x: width / 2; origin.y: clock.r * 0.92; angle: index * 30 }
        }
    }
    component Hand: Rectangle {
        property real angle
        property real length
        x: clock.r - width / 2; y: clock.r - length
        height: length + clock.r * 0.1
        radius: width / 2
        antialiasing: true
        transform: Rotation { origin.x: width / 2; origin.y: length; angle: parent ? angle : 0 }
    }
    Hand { width: clock.r * 0.09; length: clock.r * 0.5; color: clock.ink; angle: clock.hours * 30 }
    Hand { width: clock.r * 0.06; length: clock.r * 0.76; color: clock.ink; angle: clock.minutes * 6 }
    Rectangle {
        width: clock.r * 0.14; height: width; radius: width / 2
        anchors.centerIn: parent
        color: clock.accent
    }
}
