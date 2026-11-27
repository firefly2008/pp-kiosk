// Achtergrond van het Glas-thema: donker, met zachte lichtgevende kleurvlekken die heel langzaam bewegen.
// De vlekken zijn vooraf vervaagde plaatjes, dus licht voor een eenvoudige pc (geen live blur).
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

Rectangle {
    id: glow
    color: "#0A0F1E"
    property real s: Math.min(width / 1920, height / 1080)
    property bool animate: visible
    component Blob: Image {
        id: blob
        property real fx: 0.5
        property real fy: 0.5
        property real dx: 0.08
        property int period: 40000
        property real t: 0
        width: 1100 * glow.s; height: width
        x: (fx + dx * Math.sin(t * 2 * Math.PI)) * glow.width - width / 2
        y: (fy + dx * Math.cos(t * 2 * Math.PI)) * glow.height - height / 2
        smooth: true
        opacity: 0.75
        NumberAnimation on t { from: 0; to: 1; duration: blob.period; loops: Animation.Infinite; running: glow.animate }
    }
    Blob { source: "qrc:/glass/violet.png"; fx: 0.15; fy: 0.2; period: 47000 }
    Blob { source: "qrc:/glass/blauw.png"; fx: 0.85; fy: 0.25; period: 53000; dx: 0.06 }
    Blob { source: "qrc:/glass/turquoise.png"; fx: 0.3; fy: 0.95; period: 61000 }
    Blob { source: "qrc:/glass/roze.png"; fx: 0.95; fy: 0.9; period: 43000; dx: 0.05; opacity: 0.55 }
}
