// Telefoon als afstandsbediening (4.4): aan/uit, QR-code om te scannen, adres en code.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

FocusScope {
    id: ro

    readonly property real unit: height / 100
    property Item returnFocusTo: null

    anchors.fill: parent
    visible: false
    z: 320

    function open(focusItem) {
        returnFocusTo = focusItem
        visible = true
        toggleButton.forceActiveFocus()
        console.info("Remote overlay; enabled", remote.enabled, remote.address)
    }
    function close() {
        visible = false
        if (returnFocusTo) returnFocusTo.forceActiveFocus()
    }
    Keys.onEscapePressed: close()
    Keys.onBackPressed: close()

    Rectangle { anchors.fill: parent; color: "#f00b0c0f"; MouseArea { anchors.fill: parent } }

    Row {
        anchors.centerIn: parent
        spacing: ro.unit * 6

        // QR-code
        Rectangle {
            width: ro.unit * 42
            height: width
            radius: ro.unit * 1.5
            color: remote.listening ? "white" : "#1d2026"
            Image {
                anchors.fill: parent
                anchors.margins: ro.unit * 1.5
                visible: remote.listening
                source: remote.qrImage
                fillMode: Image.PreserveAspectFit
                smooth: false
                cache: false
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.centerIn: parent
                width: parent.width * 0.8
                visible: !remote.listening
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: "#a8acb3"
                font.pixelSize: ro.unit * 2.4 * appModel.fontScale
                text: qsTr("Zet de afstandsbediening aan; dan verschijnt hier een QR-code.")
            }
        }

        Column {
            width: ro.unit * 70
            anchors.verticalCenter: parent.verticalCenter
            spacing: ro.unit * 2
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                color: "white"
                font.pixelSize: ro.unit * 4.5 * appModel.fontScale
                font.weight: Font.DemiBold
                text: qsTr("Telefoon als afstandsbediening")
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                wrapMode: Text.Wrap
                color: "#c9ccd1"
                lineHeight: 1.25
                font.pixelSize: ro.unit * 2.4 * appModel.fontScale
                text: remote.listening
                      ? qsTr("1. Zorg dat je telefoon op hetzelfde wifi-netwerk zit.\n2. Scan de QR-code met de camera van je telefoon.\n3. Bedien Kijkluister met de pijltjes, OK, Terug en Home, en typ met het toetsenbord van je telefoon.\n\nAls app: tik in Chrome op ⋮ → Toevoegen aan startscherm. Dan staat Kijkluister tussen je apps en onthoudt hij de code.")
                      : qsTr("Bedien Kijkluister met je telefoon: pijltjes, OK, Terug, Home, volume en typen. Werkt alleen op je eigen wifi en alleen met de code.")
            }
            Column {
                visible: remote.listening
                spacing: ro.unit * 0.6
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    color: "#a8acb3"
                    font.pixelSize: ro.unit * 2 * appModel.fontScale
                    text: qsTr("Of typ dit adres in de browser van je telefoon:")
                }
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    color: "#ffb74d"
                    font.pixelSize: ro.unit * 3 * appModel.fontScale
                    font.weight: Font.DemiBold
                    text: remote.address + "/?c=" + remote.code
                }
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    color: "#a8acb3"
                    font.pixelSize: ro.unit * 2 * appModel.fontScale
                    text: remote.clients > 0 ? qsTr("📱 Verbonden met ") + remote.clients + (remote.clients === 1 ? qsTr(" telefoon") : qsTr(" telefoons"))
                                             : qsTr("Nog geen telefoon verbonden")
                }
            }
            Row {
                spacing: ro.unit * 1.2
                FocusButton {
                    id: toggleButton
                    unit: ro.unit
                    text: remote.enabled ? qsTr("Uitzetten") : qsTr("Aanzetten")
                    onClicked: remote.enabled = !remote.enabled
                    KeyNavigation.right: codeButton.visible ? codeButton : closeButton
                }
                FocusButton {
                    id: codeButton
                    visible: remote.listening
                    unit: ro.unit
                    text: qsTr("Nieuwe code")
                    onClicked: remote.newCode()
                    KeyNavigation.left: toggleButton
                    KeyNavigation.right: closeButton
                }
                FocusButton {
                    id: closeButton
                    unit: ro.unit
                    text: qsTr("Sluiten")
                    onClicked: ro.close()
                    KeyNavigation.left: codeButton.visible ? codeButton : toggleButton
                }
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                visible: remote.listening
                width: parent.width
                wrapMode: Text.Wrap
                color: "#80848b"
                font.pixelSize: ro.unit * 1.8 * appModel.fontScale
                text: qsTr("Nieuwe code: telefoons die eerder verbonden waren werken dan niet meer tot je opnieuw scant.")
            }
        }
    }
}
