// Knop "Hulp" (?) in de bovenbalk: welke knop doet wat, op controller, afstandsbediening en toetsenbord.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

FocusScope {
    id: help

    readonly property real unit: height / 100
    property Item returnFocusTo: null
    readonly property var rows: [
        { what: qsTr("Kiezen"), pad: qsTr("Pijltjes / joystick"), remote: qsTr("Pijltjes"), key: qsTr("Pijltjes") },
        { what: qsTr("Openen"), pad: "A", remote: "OK", key: qsTr("Enter") },
        { what: qsTr("Terug"), pad: "B", remote: qsTr("Terug"), key: qsTr("Esc") },
        { what: qsTr("Opties van een tegel (verplaatsen, kinderslot, ...)"), pad: qsTr("A vasthouden of ☰"), remote: qsTr("Menu / blauw"), key: qsTr("Enter vasthouden of F2") },
        { what: qsTr("Naar het startscherm, vanuit elke app of website"), pad: qsTr("Home / Xbox-knop"), remote: qsTr("Home"), key: qsTr("Esc vasthouden (instelbaar)") },
        { what: qsTr("Website: vorige pagina"), pad: "B", remote: qsTr("Terug"), key: qsTr("Alt + ←") },
        { what: qsTr("Tegel 1 t/m 9 van de bovenste rij openen"), pad: "—", remote: qsTr("Cijfers 1–9"), key: "1–9" },
        { what: qsTr("Live tv: zender kiezen op nummer"), pad: "—", remote: qsTr("Cijfers"), key: qsTr("Cijfers") },
        { what: qsTr("Live tv: tv-gids / opnemen"), pad: qsTr("Y / RB"), remote: qsTr("Gids / rood"), key: qsTr("I / R") },
        { what: qsTr("Live tv: gids van alle zenders"), pad: "—", remote: "—", key: "G" },
        { what: qsTr("Live tv: vorige zender / zappen"), pad: "—", remote: qsTr("0 / P+ P−"), key: qsTr("0 / PgUp PgDn") },
        { what: qsTr("Tv-gids: herinnering / serie opnemen"), pad: "—", remote: "—", key: qsTr("H / S") },
        { what: qsTr("Zoeken"), pad: qsTr("Zoeken (bovenbalk)"), remote: qsTr("Typen of 🎤"), key: qsTr("Gewoon typen") },
        { what: qsTr("Radio: andere zender / volume"), pad: "◀ ▶ / ▲ ▼", remote: "◀ ▶ / ▲ ▼", key: "◀ ▶ / ▲ ▼" },
        { what: qsTr("Slaaptimer"), pad: qsTr("Y in de radio"), remote: qsTr("Menu → Slaaptimer"), key: qsTr("T in de radio") }
    ]

    anchors.fill: parent
    visible: false
    z: 120

    function open(focusItem) {
        returnFocusTo = focusItem
        visible = true
        forceActiveFocus()
        console.info("Help opened")
    }

    function close() {
        visible = false
        if (returnFocusTo) returnFocusTo.forceActiveFocus()
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back || event.key === Qt.Key_Backspace
                || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            close()
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#f20b0c0f"
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onClicked: help.close() }
    }

    Column {
        x: help.unit * 6
        y: help.unit * 5
        width: parent.width - help.unit * 12
        spacing: help.unit * 1.2

        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            text: qsTr("Hulp: welke knop doet wat?")
            color: "white"
            font.pixelSize: help.unit * 4.2 * appModel.fontScale
            font.weight: Font.DemiBold
        }

        // Kopregel
        Row {
            width: parent.width
            Repeater {
                model: ["", qsTr("🎮 Controller"), qsTr("📺 Afstandsbediening"), qsTr("⌨ Toetsenbord")]
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    required property string modelData
                    required property int index
                    width: index === 0 ? parent.width * 0.4 : parent.width * 0.2
                    text: modelData
                    color: "#a8acb3"
                    font.pixelSize: help.unit * 2.2 * appModel.fontScale
                    font.weight: Font.DemiBold
                }
            }
        }

        Repeater {
            model: help.rows
            Rectangle {
                required property var modelData
                required property int index
                width: parent.width
                height: help.unit * 5.4
                radius: help.unit * 0.8
                color: index % 2 === 0 ? "#161a21" : "transparent"
                Row {
                    anchors.fill: parent
                    anchors.leftMargin: help.unit
                    Repeater {
                        model: [modelData.what, modelData.pad, modelData.remote, modelData.key]
                        Text {
                            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                            required property string modelData
                            required property int index
                            width: index === 0 ? parent.width * 0.4 : parent.width * 0.2
                            height: parent.height
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            text: modelData
                            color: index === 0 ? "white" : "#e6e8eb"
                            font.pixelSize: help.unit * 2.2 * appModel.fontScale
                        }
                    }
                }
            }
        }

        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            topPadding: help.unit * 1.5
            text: qsTr("Druk op B, Esc of Enter om te sluiten.")
            color: "#a8acb3"
            font.pixelSize: help.unit * 2.2 * appModel.fontScale
        }
    }
}
