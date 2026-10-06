import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ToolBar {
    id: root
    required property var backend
    required property var owner
    property bool playing: false
    signal newRequested()
    signal openRequested()
    signal removeRequested()
    signal playbackRequested()
    signal propertiesRequested()
    height: 46
    background: Rectangle {
        color: "#1c2022"
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#303638" }
    }
    component Command: ToolButton {
        id: command
        required property string symbol
        required property string description
        implicitWidth: 42
        implicitHeight: 36
        padding: 8
        Accessible.name: description
        contentItem: Image {
            source: "qrc:/assets/ui/" + command.symbol + ".svg"
            sourceSize: Qt.size(22, 22)
            fillMode: Image.PreserveAspectFit
            opacity: command.enabled ? 1 : 0.35
        }
        background: Rectangle {
            radius: 4
            color: command.down ? "#363c3f" : command.hovered ? "#2b3134" : "transparent"
        }
        ToolTip.visible: hovered
        ToolTip.delay: 600
        ToolTip.text: description
    }
    component Divider: Rectangle {
        implicitWidth: 1
        implicitHeight: 28
        color: "#303638"
        Layout.leftMargin: 5
        Layout.rightMargin: 5
    }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 16
        spacing: 2
        Command { symbol: "new"; description: "New project · Ctrl+N"; onClicked: root.newRequested() }
        Divider {}
        Command { symbol: "open"; description: "Open client folder · Ctrl+O"; onClicked: root.openRequested() }
        Command { symbol: "save"; description: "Compile DAT + SPR · Ctrl+S"; enabled: root.backend.loaded; onClicked: root.backend.compile() }
        Divider {}
        Command { symbol: "add"; description: "New object"; enabled: root.backend.loaded; onClicked: root.backend.create() }
        Command { symbol: "copy"; description: "Duplicate object · Ctrl+D"; enabled: root.owner.objectEditable; onClicked: root.backend.create(true) }
        Divider {}
        Command { symbol: "remove"; description: "Remove selected object"; enabled: root.owner.objectEditable; onClicked: root.removeRequested() }
        Divider {}
        Command { symbol: "undo"; description: "Undo · Ctrl+Z"; enabled: root.backend.canUndo; onClicked: root.backend.undo() }
        Command { symbol: "redo"; description: "Redo · Ctrl+Y"; enabled: root.backend.canRedo; onClicked: root.backend.redo() }
        Divider {}
        Command { symbol: root.playing ? "pause" : "play"; description: "Play / pause animation"; enabled: root.backend.loaded; onClicked: root.playbackRequested() }
        Divider {}
        Command { symbol: "settings"; description: "Item properties"; enabled: root.owner.editable; onClicked: root.propertiesRequested() }
        Divider {}
        Item { Layout.fillWidth: true }
        Label { text: "DAT / SPR  \u2022  QML \u2022 C++"; color: "#d4dce2"; font.pixelSize: 12 }
        Rectangle { implicitWidth: 7; implicitHeight: 7; radius: 4; color: root.backend.dirty ? "#e7b467" : "#c6d4e4"; Layout.leftMargin: 14; Layout.rightMargin: 10 }
        ToolButton {
            id: projectButton
            implicitHeight: 34
            padding: 8
            Accessible.name: "Project actions"
            ToolTip.visible: hovered
            ToolTip.text: root.backend.loaded ? root.backend.info.folder : "Open a project"
            contentItem: RowLayout {
                spacing: 12
                Label {
                    text: root.backend.loaded ? "Project: " + root.backend.info.folder.toString().replace(/\\/g, "/").split("/").filter(function(part) { return part.length > 0 }).pop() + "/" : "No project loaded"
                    color: "#d4dce2"
                    font.pixelSize: 12
                    elide: Text.ElideMiddle
                    Layout.maximumWidth: 250
                }
                Image { source: "qrc:/assets/ui/chevron.svg"; sourceSize: Qt.size(16, 16); Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
            }
            background: Rectangle { radius: 4; color: projectButton.hovered ? "#2b3134" : "transparent" }
            onClicked: projectMenu.open()
            Menu {
                id: projectMenu
                y: projectButton.height
                MenuItem { text: "New project…"; onTriggered: root.newRequested() }
                MenuItem { text: "Open project…"; onTriggered: root.openRequested() }
                MenuItem { text: "Compile DAT + SPR"; enabled: root.backend.loaded; onTriggered: root.backend.compile() }
            }
        }
    }
}
