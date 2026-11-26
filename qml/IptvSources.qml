// Bronnen voor live tv beheren: M3U-link, M3U-bestand of Xtream Codes-account
// (de drie soorten die open-tv ook kent).
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Dialogs

FocusScope {
    id: sources

    readonly property real unit: height / 100
    property string addType: "m3u-url"
    property int confirmRemove: -1
    signal closed()

    anchors.fill: parent
    visible: false
    z: 100

    function open() {
        addType = "m3u-url"
        nameField.text = ""
        urlField.text = ""
        userField.text = ""
        passwordField.text = ""
        warning.text = ""
        epgField.text = iptv.extraEpgUrls.join(", ")
        confirmRemove = -1
        visible = true
        const first = sourceList.count > 0 ? sourceList : urlTab
        first.forceActiveFocus()
    }

    function close() {
        visible = false
        closed()
    }

    function add() {
        if (iptv.addSource(addType, nameField.text, urlField.text, userField.text, passwordField.text)) {
            close()
        } else {
            warning.text = addType === "xtream" ? qsTr("Vul server, gebruikersnaam en wachtwoord in.")
                         : addType === "m3u-file" ? qsTr("Kies het bestand met de zenderlijst (eindigt meestal op .m3u).")
                         : qsTr("Vul de link in die je van je tv-aanbieder kreeg (begint met http).")
        }
    }

    // Voorbeeld voor de handleiding
    function demoXtream() {
        addType = "xtream"
        nameField.text = qsTr("Mijn abonnement")
        urlField.text = "http://voorbeeld.tv:8080"
        userField.text = "gebruiker"
        passwordField.text = "geheim123"
        addButton.forceActiveFocus()
    }

    Keys.onEscapePressed: close()

    // Invoerveld dat met pijltjes bereikbaar is
    component Field: FocusScope {
        id: field
        property alias text: input.text
        property string placeholder
        property bool secret: false
        property real unit: 10
        property Item upItem: null
        property Item downItem: null
        height: unit * 6
        Rectangle {
            anchors.fill: parent
            radius: field.unit
            color: "#0f1115"
            border.width: input.activeFocus ? 3 : 1
            border.color: input.activeFocus ? "#e8eaed" : "#33ffffff"
        }
        TextInput {
            id: input
            anchors.fill: parent
            anchors.margins: field.unit * 1.5
            focus: true
            verticalAlignment: TextInput.AlignVCenter
            color: "white"
            font.pixelSize: (field.unit * 2.3) * appModel.fontScale
            clip: true
            selectByMouse: true
            echoMode: field.secret ? TextInput.Password : TextInput.Normal
            KeyNavigation.up: field.upItem
            KeyNavigation.down: field.downItem
        }
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            anchors.fill: input
            verticalAlignment: Text.AlignVCenter
            visible: input.text === "" && !input.activeFocus
            color: "#a8acb3"
            font.pixelSize: (field.unit * 2.3) * appModel.fontScale
            text: field.placeholder
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("M3U-afspeellijst kiezen")
        nameFilters: [qsTr("Afspeellijsten (*.m3u *.m3u8)"), qsTr("Alle bestanden (*)")]
        onAccepted: urlField.text = iptv.localPath(selectedFile)
    }

    Rectangle {
        anchors.fill: parent
        color: "#d8000000"
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; hoverEnabled: true }
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.9, sources.unit * 120)
        height: content.height + sources.unit * 8
        radius: sources.unit * 2
        color: "#1b1d22"
        border.color: "#33ffffff"
        border.width: 1

        Column {
            id: content
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: sources.unit * 4
            spacing: sources.unit * 1.4

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                color: "white"
                font.pixelSize: (sources.unit * 4) * appModel.fontScale
                font.weight: Font.DemiBold
                text: qsTr("Bronnen voor live tv")
            }

            // --- Bestaande bronnen ---
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                visible: sourceList.count > 0
                color: "#9fb4d0"
                font.pixelSize: (sources.unit * 2.2) * appModel.fontScale
                text: qsTr("Huidige bronnen (Enter om te verwijderen)")
            }
            ListView {
                id: sourceList
                width: parent.width
                height: Math.min(count, 3) * sources.unit * 7
                visible: count > 0
                clip: true
                spacing: sources.unit
                model: iptv.sources
                keyNavigationEnabled: true
                KeyNavigation.down: urlTab

                delegate: FocusButton {
                    required property int index
                    required property var modelData
                    unit: sources.unit
                    focus: ListView.isCurrentItem
                    text: sources.confirmRemove === index
                          ? qsTr("Nogmaals Enter om ") + modelData.name + qsTr(" te verwijderen")
                          : "✕   " + modelData.name + "   (" + (modelData.type === "xtream" ? "abonnement" : modelData.type === "m3u-file" ? "bestand" : "link") + ")"
                    onClicked: {
                        if (sources.confirmRemove === index) {
                            sources.confirmRemove = -1
                            iptv.removeSource(index)
                        } else {
                            sources.confirmRemove = index
                        }
                    }
                }
            }

            // --- Nieuwe bron ---
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                color: "#9fb4d0"
                font.pixelSize: (sources.unit * 2.2) * appModel.fontScale
                text: qsTr("Zenders toevoegen van je eigen tv-abonnement")
            }
            Row {
                spacing: sources.unit * 1.2
                FocusButton {
                    id: urlTab
                    unit: sources.unit
                    text: qsTr("Link van je aanbieder")
                    checked: sources.addType === "m3u-url"
                    onClicked: sources.addType = "m3u-url"
                    KeyNavigation.right: fileTab
                    KeyNavigation.up: sourceList.count > 0 ? sourceList : null
                    KeyNavigation.down: nameField
                }
                FocusButton {
                    id: fileTab
                    unit: sources.unit
                    text: qsTr("Bestand op deze computer")
                    checked: sources.addType === "m3u-file"
                    onClicked: sources.addType = "m3u-file"
                    KeyNavigation.left: urlTab
                    KeyNavigation.right: xtreamTab
                    KeyNavigation.up: sourceList.count > 0 ? sourceList : null
                    KeyNavigation.down: nameField
                }
                FocusButton {
                    id: xtreamTab
                    unit: sources.unit
                    text: qsTr("Inloggegevens van je abonnement")
                    checked: sources.addType === "xtream"
                    onClicked: sources.addType = "xtream"
                    KeyNavigation.left: fileTab
                    KeyNavigation.up: sourceList.count > 0 ? sourceList : null
                    KeyNavigation.down: nameField
                }
            }

            Field {
                id: nameField
                width: parent.width
                unit: sources.unit
                placeholder: qsTr("Naam (mag leeg blijven)")
                upItem: urlTab
                downItem: urlField
            }
            Row {
                spacing: sources.unit * 1.2
                Field {
                    id: urlField
                    width: content.width - (fileButton.visible ? fileButton.width + parent.spacing : 0)
                    unit: sources.unit
                    placeholder: sources.addType === "xtream" ? qsTr("Serveradres uit de mail van je aanbieder, bijvoorbeeld http://voorbeeld.tv:8080")
                               : sources.addType === "m3u-file" ? qsTr("Kies het bestand met de zenderlijst")
                               : qsTr("Link van je aanbieder, bijvoorbeeld http://…  (M3U)")
                    upItem: nameField
                    downItem: fileButton.visible ? fileButton : sources.addType === "xtream" ? userField : addButton
                }
                FocusButton {
                    id: fileButton
                    visible: sources.addType === "m3u-file"
                    unit: sources.unit
                    text: qsTr("Bestand kiezen…")
                    onClicked: fileDialog.open()
                    KeyNavigation.up: nameField
                    KeyNavigation.left: urlField
                    KeyNavigation.down: addButton
                }
            }
            Field {
                id: userField
                visible: sources.addType === "xtream"
                width: parent.width
                unit: sources.unit
                placeholder: qsTr("Gebruikersnaam")
                upItem: urlField
                downItem: passwordField
            }
            Field {
                id: passwordField
                visible: sources.addType === "xtream"
                width: parent.width
                unit: sources.unit
                secret: true
                placeholder: qsTr("Wachtwoord")
                upItem: userField
                downItem: addButton
            }

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                wrapMode: Text.Wrap
                color: "#9a9ea6"
                font.pixelSize: (sources.unit * 1.9) * appModel.fontScale
                text: qsTr("Gebruik alleen afspeellijsten waarvoor je een abonnement of toestemming hebt. ")
                      + qsTr("Gegevens worden alleen op deze pc bewaard.")
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                id: warning
                visible: text !== ""
                color: "#ff8a80"
                font.pixelSize: (sources.unit * 2.2) * appModel.fontScale
            }

            Row {
                spacing: sources.unit * 1.5
                FocusButton {
                    id: addButton
                    unit: sources.unit
                    text: qsTr("Toevoegen en laden")
                    onClicked: sources.add()
                    KeyNavigation.right: freeButton
                    KeyNavigation.up: sources.addType === "xtream" ? passwordField : sources.addType === "m3u-file" ? fileButton : urlField
                    KeyNavigation.down: epgField
                }
                FocusButton {
                    id: freeButton
                    unit: sources.unit
                    text: qsTr("Gratis zenders")
                    onClicked: {
                        const n = iptv.addFreeSources(false)
                        warning.text = n > 0 ? qsTr("Gratis Nederlandse en Belgische zenders toegevoegd.") : qsTr("De gratis zenders staan er al.")
                    }
                    KeyNavigation.left: addButton
                    KeyNavigation.right: newsButton
                    KeyNavigation.up: addButton.KeyNavigation.up
                    KeyNavigation.down: epgField
                }
                FocusButton {
                    id: newsButton
                    unit: sources.unit
                    text: qsTr("+ Internationaal nieuws")
                    onClicked: {
                        const n = iptv.addFreeSources(true)
                        warning.text = n > 0 ? qsTr("Ruim 1.000 nieuwszenders uit de hele wereld toegevoegd (groep Nieuws).") : qsTr("Het internationale nieuws staat er al.")
                    }
                    KeyNavigation.left: freeButton
                    KeyNavigation.right: closeButton
                    KeyNavigation.up: addButton.KeyNavigation.up
                    KeyNavigation.down: epgField
                }
                FocusButton {
                    id: closeButton
                    unit: sources.unit
                    text: qsTr("Sluiten")
                    onClicked: sources.close()
                    KeyNavigation.left: newsButton
                    KeyNavigation.up: addButton.KeyNavigation.up
                    KeyNavigation.down: epgField
                }
            }

            // --- Tv-gids ---
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                color: "#9fb4d0"
                font.pixelSize: (sources.unit * 2.2) * appModel.fontScale
                text: qsTr("Tv-gids van je aanbieder (optioneel): link naar het gidsbestand (XMLTV); meerdere met een komma ertussen")
            }
            Row {
                spacing: sources.unit * 1.2
                Field {
                    id: epgField
                    width: content.width - epgSave.width - parent.spacing
                    unit: sources.unit
                    placeholder: qsTr("Leeg laten: de gratis zenders krijgen de gids vanzelf")
                    upItem: addButton
                    downItem: null
                    Keys.onReturnPressed: epgSave.clicked()
                }
                FocusButton {
                    id: epgSave
                    unit: sources.unit
                    text: qsTr("Gids opslaan")
                    onClicked: {
                        iptv.extraEpgUrls = epgField.text.split(/[,\n]/).map(s => s.trim()).filter(s => s !== "")
                        warning.text = qsTr("Gids wordt opnieuw opgehaald.")
                    }
                    KeyNavigation.left: epgField
                    KeyNavigation.up: addButton
                }
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                color: "#a8acb3"
                font.pixelSize: (sources.unit * 1.9) * appModel.fontScale
                text: (iptv.epgStatus !== "" ? iptv.epgStatus + "   ·   " : "")
                      + qsTr("De gratis Nederlandse en Belgische zenders krijgen elke nacht een nieuwe gids (tvgids.nl, Delta, VRT, VTM, Proximus Pickx).")
                width: content.width
                wrapMode: Text.Wrap
            }
        }
    }
}
