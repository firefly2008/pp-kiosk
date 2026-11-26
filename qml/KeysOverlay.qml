// Eigen sleutels voor gratis diensten (TMDB: films en series, Unsplash: achtergrondfoto's).
// Per dienst: waar je de sleutel aanmaakt (adres + QR-code voor de telefoon), een invoerveld en een testknop.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

FocusScope {
    id: overlay

    property Item returnFocusTo: null
    readonly property real unit: height / 100

    anchors.fill: parent
    visible: false
    z: 100

    function open(focusItem) {
        returnFocusTo = focusItem
        tmdbBlock.field.text = library.apiKey
        unsplashBlock.field.text = apiKeys.unsplashKey
        tmdbBlock.result = ""
        unsplashBlock.result = ""
        visible = true
        tmdbBlock.field.forceActiveFocus()
    }
    function close() {
        visible = false
        if (returnFocusTo) returnFocusTo.forceActiveFocus()
    }
    function save() {
        library.setApiKey(tmdbBlock.field.text)
        apiKeys.setUnsplashKey(unsplashBlock.field.text)
        close()
    }

    Keys.onEscapePressed: close()

    Connections {
        target: apiKeys
        function onTested(kind, ok, message) {
            const b = kind === "tmdb" ? tmdbBlock : unsplashBlock
            b.result = message
            b.ok = ok
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#c0000000"
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; hoverEnabled: true }
    }

    component KeyBlock: Item {
        id: block
        property string kind
        property string title
        property string purpose
        property alias field: input
        property string result: ""
        property bool ok: false
        property Item upTarget: null
        property Item downTarget: null
        width: parent.width
        height: Math.max(col.height, qr.height)

        Image {
            id: qr
            anchors.right: parent.right
            anchors.top: parent.top
            width: overlay.unit * 14; height: width
            sourceSize.width: 256; sourceSize.height: 256
            smooth: false
            source: apiKeys.qrFor(block.kind)
        }
        Column {
            id: col
            anchors.left: parent.left
            anchors.right: qr.left
            anchors.rightMargin: overlay.unit * 2
            spacing: overlay.unit * 1
            Text {
                textFormat: Text.PlainText
                color: "white"
                font.pixelSize: (overlay.unit * 3) * appModel.fontScale
                font.weight: Font.DemiBold
                text: block.title
            }
            Text {
                textFormat: Text.PlainText
                width: parent.width
                wrapMode: Text.Wrap
                color: "#9fb4d0"
                font.pixelSize: (overlay.unit * 2) * appModel.fontScale
                text: block.purpose
            }
            Text {
                textFormat: Text.PlainText
                width: parent.width
                wrapMode: Text.WrapAnywhere
                color: "#c4c7cc"
                font.pixelSize: (overlay.unit * 2) * appModel.fontScale
                text: qsTr("Sleutel aanmaken: ") + apiKeys.signupUrl(block.kind)
            }
            Rectangle {
                width: parent.width
                height: overlay.unit * 6
                radius: overlay.unit
                color: "#0f1826"
                border.width: input.activeFocus ? 3 : 1
                border.color: input.activeFocus ? "#5b9bff" : "#44ffffff"
                TextInput {
                    id: input
                    anchors.fill: parent
                    anchors.margins: overlay.unit * 1.5
                    verticalAlignment: TextInput.AlignVCenter
                    color: "white"
                    font.pixelSize: (overlay.unit * 2.3) * appModel.fontScale
                    clip: true
                    selectByMouse: true
                    echoMode: TextInput.PasswordEchoOnEdit
                    KeyNavigation.down: testButton
                    KeyNavigation.up: block.upTarget
                    onAccepted: testButton.forceActiveFocus()
                    onTextChanged: block.result = ""
                }
                Text {
                    textFormat: Text.PlainText
                    anchors.fill: input
                    verticalAlignment: Text.AlignVCenter
                    visible: input.text === "" && !input.activeFocus
                    color: "#a8acb3"
                    font.pixelSize: (overlay.unit * 2.2) * appModel.fontScale
                    text: qsTr("Plak hier je sleutel of token")
                }
            }
            Row {
                spacing: overlay.unit * 2
                FocusButton {
                    id: testButton
                    unit: overlay.unit
                    text: qsTr("Testen")
                    onClicked: { block.result = qsTr("Testen…"); block.ok = false; apiKeys.test(block.kind, input.text) }
                    KeyNavigation.up: input
                    KeyNavigation.down: block.downTarget
                }
                Text {
                    textFormat: Text.PlainText
                    anchors.verticalCenter: parent.verticalCenter
                    color: block.ok ? "#7ee2a8" : "#ffb4a8"
                    font.pixelSize: (overlay.unit * 2.2) * appModel.fontScale
                    text: block.result
                }
            }
        }
    }

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(overlay.width * 0.9, overlay.unit * 110)
        height: content.height + overlay.unit * 6
        radius: overlay.unit * 2
        color: "#1d2a40"
        border.color: "#44ffffff"
        border.width: 1

        Column {
            id: content
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: overlay.unit * 3
            spacing: overlay.unit * 2

            Text {
                textFormat: Text.PlainText
                color: "white"
                font.pixelSize: (overlay.unit * 3.8) * appModel.fontScale
                text: qsTr("Sleutels")
            }
            Text {
                textFormat: Text.PlainText
                width: parent.width
                wrapMode: Text.Wrap
                color: "#c4c7cc"
                font.pixelSize: (overlay.unit * 2.1) * appModel.fontScale
                text: qsTr("Kijkluister heeft geen eigen sleutels meer. Maak gratis een eigen sleutel aan (scan de QR-code met je telefoon). Ze worden versleuteld op deze pc bewaard.")
            }
            KeyBlock {
                id: tmdbBlock
                kind: "tmdb"
                title: qsTr("TMDB: posters en gegevens bij films en series")
                purpose: qsTr("Zonder sleutel komen series van TVmaze en tonen films alleen hun titel.")
                downTarget: unsplashBlock.field
            }
            KeyBlock {
                id: unsplashBlock
                kind: "unsplash"
                title: qsTr("Unsplash: natuurfoto's voor achtergrond en schilderij")
                purpose: qsTr("Zonder sleutel zoekt Achtergrond → Gratis foto via Openverse, en is de bron Unsplash in Schilderij niet beschikbaar.")
                upTarget: tmdbBlock.field
                downTarget: saveButton
            }
            Row {
                spacing: overlay.unit * 2
                FocusButton {
                    id: saveButton
                    unit: overlay.unit
                    text: qsTr("Bewaren")
                    onClicked: overlay.save()
                    KeyNavigation.up: unsplashBlock.field
                    KeyNavigation.right: cancelButton
                }
                FocusButton {
                    id: cancelButton
                    unit: overlay.unit
                    text: qsTr("Terug")
                    onClicked: overlay.close()
                    KeyNavigation.up: unsplashBlock.field
                    KeyNavigation.left: saveButton
                }
            }
        }
    }
}
