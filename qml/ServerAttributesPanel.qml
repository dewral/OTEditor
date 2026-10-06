import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property var editor
    property bool advancedVisible: false
    readonly property var draft: editor.serverDraft
    spacing: 10

    component ServerFlag: CheckBox {
        id: flagControl
        required property string field
        objectName: "serverFlag_" + field
        checked: Boolean(panel.draft[field])
        onClicked: panel.editor.editServer(field, checked)
        padding: 0
        spacing: 7
        implicitHeight: 28
        font.pixelSize: 12
        indicator: Rectangle {
            width: 16; height: 16
            y: (flagControl.height - height) / 2
            color: flagControl.checked ? "#399ee8" : "#1c2022"
            border.color: "#9aabb9"
            Label { anchors.centerIn: parent; text: "✓"; font.pixelSize: 14; visible: flagControl.checked; color: "#ffffff" }
        }
    }

    GroupBox {
        title: "Attributes"
        Layout.fillWidth: true
        padding: 12
        GridLayout {
            anchors.fill: parent
            columns: panel.width >= 560 ? 2 : 1
            columnSpacing: 18
            rowSpacing: 12

            RowLayout {
                Layout.alignment: Qt.AlignTop
                spacing: 18
                ColumnLayout {
                    spacing: 0
                    Repeater {
                        model: [["Unpassable", "unpassable"], ["Movable", "moveable"],
                                ["Block Missiles", "blockMissiles"], ["Block Pathfinder", "blockPathfinder"],
                                ["Pickupable", "pickupable"], ["Stackable", "stackable"],
                                ["Force Use", "forceUse"], ["Multi Use", "useable"],
                                ["Rotatable", "rotatable"], ["Hangable", "hangable"],
                                ["Hook South", "hookSouth"], ["Hook East", "hookEast"]]
                        ServerFlag { required property var modelData; text: modelData[0]; field: modelData[1] }
                    }
                }
                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    spacing: 0
                    Repeater {
                        model: [["Has Elevation", "hasElevation"], ["Ignore Look", "ignoreLook"],
                                ["Readable", "readable"], ["Full Ground", "fullGround"]]
                        ServerFlag { required property var modelData; text: modelData[0]; field: modelData[1] }
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                columns: 2
                columnSpacing: 6
                rowSpacing: 6
                Repeater {
                    model: [["Ground Speed", "speed", 65535], ["Minimap Color", "minimapColor", 65535],
                            ["Light Level", "lightLevel", 255], ["Light Color", "lightColor", 255],
                            ["Ware ID", "wareId", 65535], ["Read Length", "maxReadLength", 65535],
                            ["Read / Write Length", "maxReadWriteLength", 65535]]
                    RowLayout {
                        required property var modelData
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        spacing: 6
                        Label { text: modelData[0] + ":"; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight; font.pixelSize: 12 }
                        SpinBox {
                            objectName: "serverValue_" + modelData[1]
                            Layout.minimumWidth: 132
                            Layout.preferredWidth: 132
                            implicitHeight: 30
                            from: 0; to: modelData[2]; editable: true
                            value: Number(panel.draft[modelData[1]] || 0)
                            onValueModified: panel.editor.editServer(modelData[1], value)
                        }
                    }
                }
                Label { text: "Stack Order:"; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight; font.pixelSize: 12 }
                ComboBox {
                    id: stackOrder
                    objectName: "serverStackOrder"
                    Layout.preferredWidth: 100
                    Layout.fillWidth: true
                    implicitHeight: 30
                    model: ["None", "Border", "Bottom", "Top"]
                    currentIndex: Number(panel.draft.stackOrder || 0)
                    onActivated: {
                        panel.editor.editServer("stackOrder", currentIndex)
                        panel.editor.editServer("alwaysOnTop", currentIndex !== 0)
                    }
                }
                Label { text: "Name:"; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight; font.pixelSize: 12 }
                TextField {
                    objectName: "serverItemName"
                    Layout.preferredWidth: 100
                    Layout.fillWidth: true
                    implicitHeight: 30
                    text: String(panel.draft.name || "")
                    selectByMouse: true
                    onTextEdited: panel.editor.editServer("name", text)
                }
                Label { text: "Type:"; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight; font.pixelSize: 12 }
                ComboBox {
                    objectName: "serverItemType"
                    Layout.preferredWidth: 100
                    Layout.fillWidth: true
                    implicitHeight: 30
                    model: ["None", "Ground", "Container", "Weapon", "Ammunition", "Armor", "Changes",
                            "Teleport", "Magic Field", "Writeable", "Key", "Splash", "Fluid", "Door", "Deprecated", "Podium"]
                    currentIndex: Number(panel.draft.groupId || 0)
                    onActivated: panel.editor.editServer("groupId", currentIndex)
                }
            }
        }
    }

    ToolButton {
        text: (panel.advancedVisible ? "▾ " : "▸ ") + "Identity and additional flags"
        onClicked: panel.advancedVisible = !panel.advancedVisible
    }
    ColumnLayout {
        visible: panel.advancedVisible
        Layout.fillWidth: true
        Repeater {
            model: [["Server ID", "serverId", 1], ["Client ID", "clientId", 100]]
            RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Label { text: modelData[0]; Layout.fillWidth: true }
                SpinBox { from: modelData[2]; to: 65535; editable: true; value: Number(panel.draft[modelData[1]] || modelData[2]); onValueModified: panel.editor.editServer(modelData[1], value) }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Description" }
            TextField { Layout.fillWidth: true; text: String(panel.draft.description || ""); onTextEdited: panel.editor.editServer("description", text) }
        }
        Flow {
            Layout.fillWidth: true
            spacing: 12
            Repeater {
                model: [["Distance Read", "allowDistRead"], ["Client Duration", "clientDuration"],
                        ["Client Charges", "clientCharges"], ["Animation", "animation"]]
                ServerFlag { required property var modelData; text: modelData[0]; field: modelData[1] }
            }
        }
    }
}
