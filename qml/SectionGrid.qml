// Een rij op de startpagina (Video, Muziek, Podcasts, Media):
// titel met tegels eronder. De hoogte groeit mee met het aantal tegels; de
// startpagina scrolt. Bij de rand van de rij geeft hij de focus door.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

Item {
    id: section

    property string title
    property var model: null
    property int columns: 5
    property real unit: 10
    // Tabbladen-layout: één rij met plaatjes die zijwaarts scrolt, de naam onder elk plaatje
    property bool strip: false
    // Mag de muis de selectie overnemen? (bij open venster of stilstaande muis niet)
    property var canHover: function() { return true }
    readonly property alias grid: grid
    readonly property int count: model ? model.count : 0
    // Ingeklapte rij: alleen de titel ("Podcasts ▸ 5"); klapt open zodra je erin staat
    property bool collapsed: false
    readonly property bool folded: collapsed && !grid.activeFocus

    signal exitUp(int column)
    signal exitDown(int column)
    signal launchRequested(int index)
    signal menuRequested(int index)
    signal tileFocused()

    // Focus binnenkomen in een bepaalde kolom (van boven of van onder)
    function enter(column, fromBelow) {
        if (count === 0) {
            return false
        }
        const rows = strip ? 1 : Math.ceil(count / columns)
        const row = fromBelow ? rows - 1 : 0
        grid.currentIndex = Math.min(count - 1, (strip ? 0 : row * columns) + Math.max(0, column))
        grid.forceActiveFocus()
        return true
    }

    visible: count > 0
    height: visible ? label.height + grid.height + unit * 1.5 : 0

    Text {
        textFormat: Text.PlainText   // geen HTML uit zenderlijsten/gidsen uitvoeren
        id: label
        anchors.left: parent.left
        anchors.leftMargin: section.unit * 1.5
        color: "white"
        font.pixelSize: (section.unit * 2.6) * appModel.fontScale
        font.weight: Font.DemiBold
        text: section.folded ? section.title + "   ▸ " + section.count : section.title
        opacity: section.folded ? 0.75 : 1
    }

    GridView {
        id: grid
        readonly property real pad: section.unit * 2.2
        anchors.top: label.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: section.unit * 0.5
        leftMargin: pad
        rightMargin: pad
        topMargin: pad * 0.6
        bottomMargin: pad * 0.6
        flow: section.strip ? GridView.FlowTopToBottom : GridView.FlowLeftToRight
        cellWidth: section.strip ? Math.floor(section.unit * 30) : Math.floor((width - pad * 2) / section.columns)
        cellHeight: section.strip ? Math.floor(section.unit * 30 * 0.5625 + section.unit * 7.5) : Math.floor(cellWidth * 0.46)
        // (strip: precies één rij; een paar pixels extra tegen afrondingsfouten)
        height: section.folded ? 0 : section.strip ? cellHeight + pad * 1.2 + 2
                                                   : Math.ceil(section.count / section.columns) * cellHeight + pad * 1.2
        visible: !section.folded || activeFocus
        Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        interactive: false
        keyNavigationEnabled: true
        highlightMoveDuration: 150
        model: section.model

        onActiveFocusChanged: if (activeFocus) section.tileFocused()
        onCurrentIndexChanged: {
            if (section.strip && currentIndex >= 0) positionViewAtIndex(currentIndex, GridView.Contain)
            if (activeFocus) section.tileFocused()
        }

        Component {
            id: bannerDelegate
            BannerTile {
                width: grid.cellWidth
                height: grid.cellHeight
                // Ook zichtbaar als Kijkluister (nog) niet het actieve venster is, bv. direct na het opstarten
                selected: grid.currentIndex === index && (grid.activeFocus || (Qt.application.state !== Qt.ApplicationActive && root.currentSection === section))
                backdrop: section.model && section.model.backdropAt ? section.model.backdropAt(index) : ""
                subtitle: section.model && section.model.subtitleAt ? section.model.subtitleAt(index) : ""
                progress: section.model && section.model.progressAt ? section.model.progressAt(index) : -1
                badge: section.model === appModel && appModel.pathAt(index) === "bigscreen:iptv"
                       && (recorder.recording || recorder.schedule.length > 0)
                onActivated: {
                    grid.currentIndex = index
                    grid.forceActiveFocus()
                    section.launchRequested(index)
                }
                onHovered: {
                    if (section.canHover()) {
                        grid.currentIndex = index
                        grid.forceActiveFocus()
                    }
                }
                onMenuRequested: {
                    grid.currentIndex = index
                    section.menuRequested(index)
                }
                // Slepen: de tegel neemt de plek in van de tegel waar de muis boven hangt
                onDragMoved: (x, y) => {
                    const pos = mapToItem(grid.contentItem, x, y)
                    const to = grid.indexAt(pos.x, pos.y)
                    if (to >= 0 && to !== index) {
                        section.model.move(index, to)
                        grid.currentIndex = to
                    }
                }
            }
        }
        Component {
            id: thumbDelegate
            ThumbTile {
                width: grid.cellWidth
                height: grid.cellHeight
                selected: grid.currentIndex === index && (grid.activeFocus || (Qt.application.state !== Qt.ApplicationActive && root.currentSection === section))
                backdrop: section.model && section.model.backdropAt ? section.model.backdropAt(index) : ""
                subtitle: section.model && section.model.subtitleAt ? section.model.subtitleAt(index) : ""
                progress: section.model && section.model.progressAt ? section.model.progressAt(index) : -1
                badge: section.model === appModel && appModel.pathAt(index) === "bigscreen:iptv"
                       && (recorder.recording || recorder.schedule.length > 0)
                onActivated: {
                    grid.currentIndex = index
                    grid.forceActiveFocus()
                    section.launchRequested(index)
                }
                onHovered: {
                    if (section.canHover()) {
                        grid.currentIndex = index
                        grid.forceActiveFocus()
                    }
                }
                onMenuRequested: {
                    grid.currentIndex = index
                    section.menuRequested(index)
                }
            }
        }
        delegate: section.strip ? thumbDelegate : bannerDelegate

        move: Transition { NumberAnimation { properties: "x,y"; duration: 160; easing.type: Easing.OutCubic } }
        displaced: Transition { NumberAnimation { properties: "x,y"; duration: 160; easing.type: Easing.OutCubic } }

        // Kort Enter = openen; Enter ingedrukt houden = opties (net als A ingedrukt houden)
        property bool holdHandled: false
        property bool enterDown: false     // alleen loslaten telt als het indrukken ook hier was
        Keys.onReturnPressed: (event) => grid.enterPressed(event)
        Keys.onEnterPressed: (event) => grid.enterPressed(event)
        Keys.onReleased: (event) => {
            if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !event.isAutoRepeat) {
                if (enterDown && !holdHandled) section.launchRequested(currentIndex)
                holdHandled = false
                enterDown = false
                event.accepted = true
            }
        }
        function enterPressed(event) {
            event.accepted = true
            if (!event.isAutoRepeat) {
                enterDown = true
            } else if (enterDown && !holdHandled) {
                holdHandled = true
                section.menuRequested(currentIndex)
            }
        }
        Keys.onMenuPressed: section.menuRequested(currentIndex)
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_F2) {
                section.menuRequested(currentIndex)
                event.accepted = true
            }
        }
        Keys.onUpPressed: (event) => {
            if (section.strip) {
                section.exitUp(currentIndex)
            } else if (currentIndex < section.columns) {
                section.exitUp(currentIndex % section.columns)
            } else {
                event.accepted = false
            }
        }
        Keys.onDownPressed: (event) => {
            if (section.strip) {
                section.exitDown(currentIndex)
                return
            }
            const column = currentIndex % section.columns
            const lastRowStart = Math.floor((section.count - 1) / section.columns) * section.columns
            if (currentIndex >= lastRowStart) {
                section.exitDown(column)
            } else if (currentIndex + section.columns > section.count - 1) {
                currentIndex = section.count - 1 // onvolledige laatste rij
            } else {
                event.accepted = false
            }
        }
    }
}
