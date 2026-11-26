// Eerste gebruik (en later via "Mappen en sleutel"): mappen voor films en series
// kiezen, en optioneel een TMDB-sleutel invullen.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Dialogs

FocusScope {
    id: setup

    readonly property real unit: height / 100
    property var moviesPaths: []
    property var showsPaths: []
    property bool firstRun: false
    signal saved()
    signal cancelled()

    anchors.fill: parent
    visible: false
    z: 100

    // De eigen TMDB-sleutel staat altijd zichtbaar (ook te wijzigen via Instellingen → Sleutels)
    readonly property bool showKey: true

    function open(isFirstRun) {
        firstRun = isFirstRun
        moviesPaths = library.moviesFolders.slice()
        showsPaths = library.showsFolders.slice()
        keyField.text = library.apiKey
        warning.text = ""
        visible = true
        moviesButton.forceActiveFocus()
    }

    function addPath(kind, path) {
        if (path === "") return
        const list = kind === "movies" ? moviesPaths.slice() : showsPaths.slice()
        if (list.some(p => p.toLowerCase() === path.toLowerCase())) return
        list.push(path)
        if (kind === "movies") moviesPaths = list; else showsPaths = list
    }
    function removePath(kind, index) {
        const list = kind === "movies" ? moviesPaths.slice() : showsPaths.slice()
        list.splice(index, 1)
        if (kind === "movies") moviesPaths = list; else showsPaths = list
    }

    // Lijst met gekozen mappen; F2 / X / Delete of de knop haalt er een weg
    component FolderList: Column {
        id: fl
        property string kind
        property var paths: []
        width: content.width
        spacing: setup.unit * 0.6
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            visible: fl.paths.length === 0
            color: "#a8acb3"
            font.pixelSize: (setup.unit * 2.3) * appModel.fontScale
            text: qsTr("Nog geen map gekozen (mag leeg blijven)")
        }
        Repeater {
            model: fl.paths
            delegate: Row {
                required property string modelData
                required property int index
                spacing: setup.unit * 1.5
                Rectangle {
                    width: content.width - removeButton.width - parent.spacing
                    height: setup.unit * 5.5
                    radius: setup.unit
                    color: "#0f1115"
                    Text {
                        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                        anchors.fill: parent
                        anchors.margins: setup.unit * 1.5
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideMiddle
                        color: "white"
                        font.pixelSize: (setup.unit * 2.3) * appModel.fontScale
                        text: modelData
                    }
                }
                FocusButton {
                    id: removeButton
                    unit: setup.unit
                    text: qsTr("Weg")
                    onActiveFocusChanged: if (activeFocus) flick.ensureVisible(this)
                    onClicked: setup.removePath(fl.kind, index)
                }
            }
        }
    }

    function close() {
        visible = false
    }

    function save() {
        if (moviesPaths.length === 0 && showsPaths.length === 0) {
            warning.text = qsTr("Kies minstens één map.")
            moviesButton.forceActiveFocus()
            return
        }
        library.configureFolders(moviesPaths, showsPaths, keyField.text)
        close()
        saved()
    }

    Keys.onEscapePressed: {
        close()
        cancelled()
    }

    FolderDialog {
        id: moviesDialog
        title: qsTr("Map met films kiezen")
        onAccepted: setup.addPath("movies", library.localPath(selectedFolder))
    }
    FolderDialog {
        id: showsDialog
        title: qsTr("Map met series kiezen")
        onAccepted: setup.addPath("shows", library.localPath(selectedFolder))
    }

    Rectangle {
        anchors.fill: parent
        color: "#d8000000"
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; hoverEnabled: true }
    }

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.9, setup.unit * 125)
        height: Math.min(content.height + setup.unit * 8, setup.height * 0.94)
        radius: setup.unit * 2
        color: "#1b1d22"
        border.color: "#33ffffff"
        border.width: 1

        Flickable {
            id: flick
            anchors.fill: parent
            anchors.margins: setup.unit * 4
            contentHeight: content.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick

            // Zorg dat het item met focus in beeld blijft bij navigeren met de afstandsbediening
            function ensureVisible(item) {
                if (!item) {
                    return
                }
                const p = item.mapToItem(content, 0, 0)
                const top = p.y - setup.unit * 2
                const bottom = p.y + item.height + setup.unit * 2
                if (top < contentY) {
                    contentY = Math.max(0, top)
                } else if (bottom > contentY + height) {
                    contentY = Math.min(Math.max(0, contentHeight - height), bottom - height)
                }
            }

        Column {
            id: content
            width: flick.width
            spacing: setup.unit * 1.6

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                color: "white"
                font.pixelSize: (setup.unit * 4) * appModel.fontScale
                font.weight: Font.DemiBold
                text: setup.firstRun ? qsTr("Waar staan je films en series?") : qsTr("Mappen kiezen")
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                width: parent.width
                wrapMode: Text.Wrap
                color: "#c4c7cc"
                font.pixelSize: (setup.unit * 2.2) * appModel.fontScale
                text: qsTr("Kies de map met je films en de map met je series. Meer mappen mag ook, bijvoorbeeld op een ")
                      + qsTr("externe schijf. Kijkluister zoekt er zelf de titels, plaatjes en beschrijvingen bij.")
            }

            // Films: een of meer mappen/locaties
            Row {
                spacing: setup.unit * 2
                Text { anchors.verticalCenter: parent.verticalCenter; color: "#9fb4d0"; font.pixelSize: (setup.unit * 2.2) * appModel.fontScale; text: qsTr("Mappen met films")}
                FocusButton {
                    id: moviesButton
                    unit: setup.unit
                    text: qsTr("+ Map toevoegen…")
                    onClicked: moviesDialog.open()
                    onActiveFocusChanged: if (activeFocus) flick.ensureVisible(this)
                    KeyNavigation.down: showsButton
                }
            }
            FolderList { kind: "movies"; paths: setup.moviesPaths }

            // Series: een of meer mappen/locaties
            Row {
                spacing: setup.unit * 2
                Text { anchors.verticalCenter: parent.verticalCenter; color: "#9fb4d0"; font.pixelSize: (setup.unit * 2.2) * appModel.fontScale; text: qsTr("Mappen met series")}
                FocusButton {
                    id: showsButton
                    unit: setup.unit
                    text: qsTr("+ Map toevoegen…")
                    onClicked: showsDialog.open()
                    onActiveFocusChanged: if (activeFocus) flick.ensureVisible(this)
                    KeyNavigation.up: moviesButton
                    KeyNavigation.down: keyField
                }
            }
            FolderList { kind: "shows"; paths: setup.showsPaths }

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                                width: parent.width
                wrapMode: Text.Wrap
                color: "#9fb4d0"
                font.pixelSize: (setup.unit * 2.2) * appModel.fontScale
                text: qsTr("Eigen sleutel van TMDB (voor posters en gegevens bij films en series). Leeg laten mag.")
            }
            Rectangle {
                                width: parent.width
                height: setup.unit * 6
                radius: setup.unit
                color: "#0f1115"
                border.width: keyField.activeFocus ? 3 : 1
                border.color: keyField.activeFocus ? "#e8eaed" : "#33ffffff"
                TextInput {
                    id: keyField
                    anchors.fill: parent
                    anchors.margins: setup.unit * 1.5
                    verticalAlignment: TextInput.AlignVCenter
                    color: "white"
                    font.pixelSize: (setup.unit * 2.3) * appModel.fontScale
                    clip: true
                    selectByMouse: true
                    KeyNavigation.up: showsButton
                    KeyNavigation.down: saveButton
                    onActiveFocusChanged: if (activeFocus) flick.ensureVisible(parent)
                    onAccepted: setup.save()
                }
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    anchors.fill: keyField
                    verticalAlignment: Text.AlignVCenter
                    visible: keyField.text === "" && !keyField.activeFocus
                    color: "#a8acb3"
                    font.pixelSize: (setup.unit * 2.3) * appModel.fontScale
                    text: qsTr("Plak hier je sleutel of token")
                }
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                                width: parent.width
                wrapMode: Text.Wrap
                color: "#b0b4bb"
                font.pixelSize: (setup.unit * 1.9) * appModel.fontScale
                text: qsTr("Sleutel aanvragen: themoviedb.org/settings/api (gratis account). ")
                      + qsTr("Zonder sleutel komen series van TVmaze en tonen films alleen hun titel. ")
                      + qsTr("Dit product gebruikt de TMDB API, maar is niet door TMDB goedgekeurd of gecertificeerd.")
            }

            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                id: warning
                visible: text !== ""
                color: "#ff8a80"
                font.pixelSize: (setup.unit * 2.2) * appModel.fontScale
            }

            Row {
                spacing: setup.unit * 1.5
                FocusButton {
                    id: saveButton
                    unit: setup.unit
                    text: qsTr("Opslaan en zoeken")
                    onClicked: setup.save()
                    onActiveFocusChanged: if (activeFocus) flick.ensureVisible(this)
                    KeyNavigation.up: keyField
                    KeyNavigation.right: cancelButton
                }
                FocusButton {
                    id: cancelButton
                    unit: setup.unit
                    text: qsTr("Annuleren")
                    onClicked: {
                        setup.close()
                        setup.cancelled()
                    }
                    onActiveFocusChanged: if (activeFocus) flick.ensureVisible(this)
                    KeyNavigation.up: keyField
                    KeyNavigation.left: saveButton
                }
            }
        }
        }
    }
}
