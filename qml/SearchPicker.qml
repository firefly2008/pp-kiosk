// Zoekscherm: zoekveld + schermtoetsenbord links, resultaten rechts (met kopjes per soort).
// Gebruikt voor: alles zoeken, radiozenders toevoegen en de plaats voor het weer.
// results: [{ name, detail, icon, section, badge }]; icon = url, "glyph:X" of een emoji.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

FocusScope {
    id: picker

    property string title
    property string placeholder: qsTr("Typ om te zoeken")
    property var results: []
    property bool busy: false
    property string emptyText: qsTr("Niets gevonden")
    property string pickLabel: qsTr("Openen")
    property real iconScale: 1      // foto's: grotere voorbeelden
    property bool voice: false      // microfoonknop (Windows-dicteren)

    // Spreken: het zoekveld krijgt de focus en Windows-dicteren (Win+H) zet de tekst erin
    function startVoice() {
        field.forceActiveFocus()
        homeBridge.startDictation()
    }
    property Item returnFocusTo: null
    property alias text: field.text
    readonly property real unit: height / 100
    signal query(string text)
    signal picked(int index)
    signal secondary(int index)   // Menu-knop op een resultaat (bv. opnemen)

    anchors.fill: parent
    visible: false
    z: 70

    function open(pickerTitle, focusItem, startText) {
        title = pickerTitle
        returnFocusTo = focusItem
        field.text = startText || ""
        results = []
        visible = true
        keyboard.currentRow = 1
        keyboard.currentCol = 0
        keyboard.forceActiveFocus()
        if (field.text !== "") {
            query(field.text)
        }
    }

    function close() {
        visible = false
        if (returnFocusTo) {
            returnFocusTo.forceActiveFocus()
        }
    }

    // Resultaten in een ListModel: dan werken de kopjes (section) per soort
    onResultsChanged: {
        resultModel.clear()
        for (const r of results) {
            resultModel.append({ name: r.name || "", detail: r.detail || "", icon: r.icon || "",
                                 section: r.section || "", badge: r.badge || "" })
        }
    }
    ListModel { id: resultModel }

    // Test: OK-toets van het schermtoetsenbord
    function pressOk() {
        keyboard.press("OK")
    }

    function nameAt(index) {
        return index >= 0 && index < results.length ? results[index].name : ""
    }

    Timer {
        id: debounce
        interval: 350
        onTriggered: picker.query(field.text.trim())
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            close()
            event.accepted = true
        } else if (picker.voice && (event.key === Qt.Key_I || event.key === Qt.Key_F4)) {
            startVoice()   // Y op de controller
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#0b0c0f"
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }
    }

    Text {
        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
        id: titleLabel
        x: picker.unit * 5
        y: picker.unit * 4
        text: picker.title
        color: "white"
        font.pixelSize: picker.unit * 4 * appModel.fontScale
        font.weight: Font.DemiBold
    }

    // Zoekveld
    Rectangle {
        id: fieldBox
        anchors.left: titleLabel.left
        anchors.top: titleLabel.bottom
        anchors.topMargin: picker.unit * 2.5
        width: keyboard.width
        height: picker.unit * 7
        radius: picker.unit * 1.2
        color: "#1b1f26"
        border.color: keyboard.activeFocus ? "#5b8fd8" : "#3a3f48"
        border.width: picker.unit * 0.25
        Text {
            textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
            anchors.left: parent.left
            anchors.leftMargin: picker.unit * 2
            anchors.verticalCenter: parent.verticalCenter
            text: ""
            font.family: "Segoe MDL2 Assets"
            font.pixelSize: picker.unit * 2.8
            color: "#a8acb3"
        }
        Rectangle {
            id: micButton
            visible: picker.voice
            anchors.right: parent.right
            anchors.rightMargin: picker.unit * 0.8
            anchors.verticalCenter: parent.verticalCenter
            width: picker.unit * 5.2
            height: width
            radius: width / 2
            color: field.activeFocus ? "#e53935" : micMouse.containsMouse ? "#3d6fb4" : "#2a2f38"
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.centerIn: parent
                text: qsTr("\uE720")   // microfoon
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: picker.unit * 2.6
                color: "white"
            }
            MouseArea { id: micMouse; anchors.fill: parent; hoverEnabled: true; onClicked: picker.startVoice() }
        }
        TextInput {
            id: field
            anchors.left: parent.left
            anchors.leftMargin: picker.unit * 6.5
            anchors.right: picker.voice ? micButton.left : parent.right
            anchors.rightMargin: picker.unit * 2
            anchors.verticalCenter: parent.verticalCenter
            color: "white"
            // Na het dicteren: pijltje of Enter terug naar het toetsenbord / de resultaten
            Keys.onDownPressed: keyboard.forceActiveFocus()
            Keys.onReturnPressed: if (list.count > 0) list.forceActiveFocus()
            Keys.onEscapePressed: keyboard.forceActiveFocus()
            font.pixelSize: picker.unit * 3.2 * appModel.fontScale
            clip: true
            activeFocusOnPress: false
            onTextChanged: debounce.restart()
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                anchors.verticalCenter: parent.verticalCenter
                visible: field.text === ""
                text: picker.placeholder
                color: "#80848b"
                font.pixelSize: picker.unit * 3 * appModel.fontScale
            }
            // Knipperende cursor
            Rectangle {
                x: field.contentWidth + 2
                anchors.verticalCenter: parent.verticalCenter
                width: picker.unit * 0.25
                height: picker.unit * 3.6
                color: "white"
                visible: picker.visible && cursorBlink.on
                Timer {
                    id: cursorBlink
                    property bool on: true
                    interval: 530; repeat: true; running: picker.visible
                    onTriggered: on = !on
                }
            }
        }
    }

    OnScreenKeyboard {
        id: keyboard
        anchors.left: fieldBox.left
        anchors.top: fieldBox.bottom
        anchors.topMargin: picker.unit * 2.5
        unit: picker.unit
        target: field
        focus: true
        onExitRight: if (list.count > 0) list.forceActiveFocus()
        // OK: eerste resultaat kiezen (profiel opslaan, zender toevoegen, zoeken…)
        onAccepted: {
            if (list.count > 0) {
                console.info("Keyboard OK:", picker.nameAt(0))
                picker.picked(0)
            } else {
                list.forceActiveFocus()
            }
        }
    }

    // Resultaten
    Text {
        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
        anchors.left: list.left
        anchors.top: list.top
        anchors.topMargin: picker.unit * 2
        width: list.width
        wrapMode: Text.WordWrap
        visible: list.count === 0
        color: "#a8acb3"
        font.pixelSize: picker.unit * 2.8 * appModel.fontScale
        text: picker.busy ? qsTr("Zoeken…") : field.text.trim().length < 2 ? qsTr("Typ minstens twee letters.") : picker.emptyText
    }

    ListView {
        id: list
        anchors.left: keyboard.right
        anchors.leftMargin: picker.unit * 5
        anchors.right: parent.right
        anchors.rightMargin: picker.unit * 5
        anchors.top: fieldBox.top
        anchors.bottom: hints.top
        anchors.bottomMargin: picker.unit * 2
        clip: true
        model: resultModel
        keyNavigationEnabled: true
        highlightMoveDuration: 120
        section.property: "section"
        section.delegate: Text {
            required property string section
            visible: section !== ""
            height: section !== "" ? picker.unit * 5.5 : 0
            verticalAlignment: Text.AlignBottom
            bottomPadding: picker.unit * 0.8
            text: section
            color: "#a8acb3"
            font.pixelSize: picker.unit * 2.3 * appModel.fontScale
            font.weight: Font.DemiBold
        }
        onCountChanged: currentIndex = 0

        delegate: Rectangle {
            id: resultRow
            required property int index
            required property string name
            required property string detail
            required property string icon
            required property string badge
            readonly property bool current: list.activeFocus && list.currentIndex === index
            width: list.width
            height: picker.unit * (8.4 + 6 * (picker.iconScale - 1))
            radius: picker.unit * 1.2
            color: current ? "#3d6fb4" : "transparent"

            Item {
                id: iconBox
                anchors.left: parent.left
                anchors.leftMargin: picker.unit * 1.4
                anchors.verticalCenter: parent.verticalCenter
                width: picker.unit * 6 * picker.iconScale * (picker.iconScale > 1 ? 1.6 : 1)
                height: picker.unit * 6 * picker.iconScale
                readonly property string icon: resultRow.icon
                Rectangle {
                    anchors.fill: parent
                    radius: picker.unit
                    color: "#ffffff"
                    visible: iconImage.status === Image.Ready
                }
                Image {
                    id: iconImage
                    anchors.fill: parent
                    anchors.margins: picker.unit * 0.5
                    source: iconBox.icon.startsWith("http") || iconBox.icon.startsWith("file:") || iconBox.icon.startsWith("qrc:")
                            || iconBox.icon.startsWith("image:") ? iconBox.icon : ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    sourceSize: Qt.size(128, 128)
                }
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    anchors.centerIn: parent
                    visible: iconImage.status !== Image.Ready
                    readonly property bool emoji: iconBox.icon !== "" && !iconBox.icon.startsWith("glyph:") && !iconBox.icon.startsWith("http")
                                                  && !iconBox.icon.startsWith("file:") && !iconBox.icon.startsWith("image:") && !iconBox.icon.startsWith("qrc:")
                    text: iconBox.icon.startsWith("glyph:") ? iconBox.icon.substring(6) : emoji ? iconBox.icon : "\uE8D6"
                    font.family: emoji ? "Segoe UI Emoji" : "Segoe MDL2 Assets"
                    font.pixelSize: picker.unit * 3.4
                    color: "white"
                }
            }
            Column {
                anchors.left: iconBox.right
                anchors.leftMargin: picker.unit * 2
                anchors.right: badgeLabel.left
                anchors.rightMargin: picker.unit
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    width: parent.width
                    elide: Text.ElideRight
                    text: resultRow.name
                    color: "white"
                    font.pixelSize: picker.unit * 2.9 * appModel.fontScale
                }
                Text {
                    textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                    width: parent.width
                    elide: Text.ElideRight
                    visible: text !== ""
                    text: resultRow.detail
                    color: resultRow.current ? "#e6e8eb" : "#a8acb3"
                    font.pixelSize: picker.unit * 2.2 * appModel.fontScale
                }
            }
            Text {
                textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
                id: badgeLabel
                anchors.right: parent.right
                anchors.rightMargin: picker.unit * 2
                anchors.verticalCenter: parent.verticalCenter
                text: resultRow.badge
                color: resultRow.current ? "white" : "#a8acb3"
                font.pixelSize: picker.unit * 2.2 * appModel.fontScale
            }
            MouseArea {
                anchors.fill: parent
                onClicked: { list.currentIndex = resultRow.index; picker.picked(resultRow.index) }
            }
        }

        Keys.onLeftPressed: keyboard.forceActiveFocus()
        Keys.onReturnPressed: picker.picked(currentIndex)
        Keys.onEnterPressed: picker.picked(currentIndex)
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Menu || event.key === Qt.Key_R) {
                picker.secondary(currentIndex)
                event.accepted = true
            } else if (event.key === Qt.Key_Backspace || (event.text.length === 1 && event.text > " " && !(event.modifiers & Qt.ControlModifier))) {
                // Verder typen: terug naar het zoekveld
                keyboard.forceActiveFocus()
                if (event.key === Qt.Key_Backspace) {
                    keyboard.press("⌫")
                } else {
                    field.text += event.text
                }
                event.accepted = true
            }
        }
    }

    HintBar {
        id: hints
        anchors.left: parent.left
        anchors.leftMargin: picker.unit * 5
        anchors.bottom: parent.bottom
        anchors.bottomMargin: picker.unit * 3
        unit: picker.unit
        items: list.activeFocus
               ? [{ pad: "A", key: qsTr("Enter"), label: picker.pickLabel }, { pad: "◀", key: "◀", label: qsTr("Toetsenbord") }, { pad: "B", key: qsTr("Esc"), label: qsTr("Terug") }]
               : [{ pad: "A", key: qsTr("Typen"), label: qsTr("Letter kiezen") }, { pad: "▶", key: "▶", label: qsTr("Naar de resultaten") }, { pad: qsTr("OK"), key: qsTr("OK"), label: qsTr("Bevestigen") }]
                 .concat(picker.voice ? [{ pad: "Y", key: "F4", label: qsTr("Spreken") }] : []).concat([{ pad: "B", key: qsTr("Esc"), label: qsTr("Terug") }])
    }
}
