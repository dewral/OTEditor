import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Panel {
    id: browserPanel
    property var backend
    property var owner
    signal findRequested
    signal contextMenuRequested(real x, real y)
    signal attributesRequested
    signal assignRequested
    signal removeRequested
    function revealSelection() {
        const index = backend.visibleIndex();
        if (index >= 0)
            objects.positionViewAtIndex(index, GridView.Contain);
    }

    objectName: "objectsPanel"
    visible: owner.showObjectsPanel
    title: "Objects"
    SplitView.preferredWidth: 465
    SplitView.minimumWidth: 380
    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        RowLayout {
            spacing: 4
            Layout.fillWidth: true
            Repeater {
                model: ["List", "Grid", "Found"]
                Tool {
                    required property int index
                    required property string modelData
                    text: modelData
                    Layout.fillWidth: true
                    checked: owner.browserMode === index
                    onClicked: {
                        owner.browserMode = index;
                        if (index === 2)
                            browserPanel.findRequested();
                    }
                }
            }
        }
        ComboBox {
            id: categoryPicker
            Layout.fillWidth: true
            implicitHeight: 28
            model: ["Items", "Outfits", "Effects", "Missiles"]
            currentIndex: backend.category
            onActivated: {
                owner.frame = 0;
                owner.pattern = currentIndex === 1 ? 2 : 0;
                backend.category = currentIndex;
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Label {
                text: "Columns:"
                color: "#aab8c8"
            }
            SpinBox {
                id: objectColumns
                objectName: "objectColumns"
                from: 1
                to: 20
                value: 9
                editable: true
                implicitWidth: 104
                implicitHeight: 26
                enabled: owner.browserMode !== 0
            }
            Label {
                text: "Size:"
                color: "#aab8c8"
            }
            SpinBox {
                id: cellSize
                objectName: "objectSize"
                from: 24
                to: 128
                value: 54
                editable: true
                implicitWidth: 112
                implicitHeight: 26
            }
            Item {
                Layout.fillWidth: true
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Grid {
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    leftMargin: 3
                    rightMargin: 14
                }
                columns: Math.max(1, Math.floor(width / 58))
                spacing: 3
                visible: !backend.loaded
                Repeater {
                    model: 49
                    Rectangle {
                        width: Math.floor((parent.width - (parent.columns - 1) * parent.spacing) / parent.columns)
                        height: 56
                        color: "#293039"
                        border.color: index === 0 ? "#249be7" : "#414c57"
                        radius: 2
                    }
                }
            }
            GridView {
                id: objects
                objectName: "objectsGrid"
                clip: true
                model: backend
                anchors {
                    fill: parent
                    leftMargin: 3
                }
                readonly property int scrollbarSpace: 18
                readonly property int actualColumns: owner.browserMode === 0 ? 1 : Math.max(1, Math.min(objectColumns.value, Math.floor((width - scrollbarSpace) / (cellSize.value + 4))))
                cellWidth: owner.browserMode === 0 ? width - scrollbarSpace : Math.floor((width - scrollbarSpace) / actualColumns)
                cellHeight: cellSize.value + 20
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {
                    id: objectScrollBar
                    objectName: "objectsScrollBar"
                    policy: ScrollBar.AsNeeded
                    x: objects.width - width - 2
                    width: 12
                    minimumSize: 0.10
                    background: Rectangle {
                        color: "#29323b"
                        radius: 4
                    }
                    contentItem: Rectangle {
                        color: objectScrollBar.pressed ? "#a7c6db" : "#6f899f"
                        radius: 4
                    }
                }
                delegate: Rectangle {
                    required property int objectId
                    required property int sourceRow
                    required property string imageSource
                    required property string description
                    readonly property bool multiSelected: {
                        backend.selectionRevision;
                        return backend.isSelected(sourceRow);
                    }
                    objectName: "objectCell" + objectId
                    width: objects.cellWidth - 3
                    height: objects.cellHeight - 3
                    clip: true
                    color: multiSelected ? "#174e73" : mouse.containsMouse ? "#35404b" : "#2b3138"
                    border.color: multiSelected ? "#38aaf2" : "#444b54"
                    border.width: multiSelected ? 2 : 1
                    radius: 1
                    Image {
                        source: imageSource
                        smooth: false
                        width: cellSize.value
                        height: width
                        x: owner.browserMode === 0 ? 4 : Math.round((parent.width - width) / 2)
                        y: 3
                        fillMode: Image.PreserveAspectFit
                    }
                    Label {
                        text: objectId
                        color: "#d2e4f5"
                        font.pixelSize: 11
                        x: owner.browserMode === 0 ? cellSize.value + 12 : 0
                        y: owner.browserMode === 0 ? 7 : parent.height - 18
                        width: owner.browserMode === 0 ? 80 : parent.width
                        horizontalAlignment: owner.browserMode === 0 ? Text.AlignLeft : Text.AlignHCenter
                    }
                    Label {
                        visible: owner.browserMode === 0
                        x: cellSize.value + 12
                        y: 25
                        text: description
                        font.pixelSize: 10
                        color: "#8499ac"
                    }
                    MouseArea {
                        id: mouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: function (event) {
                            if (event.button === Qt.RightButton) {
                                if (backend.isSelected(sourceRow))
                                    backend.activateSelected(sourceRow);
                                else
                                    backend.selected = sourceRow;
                                let point = mapToItem(owner.contentItem, event.x, event.y);
                                browserPanel.contextMenuRequested(point.x, point.y);
                            } else
                                backend.selectWithModifiers(sourceRow, event.modifiers);
                            owner.frame = 0;
                            owner.pattern = backend.category === 1 ? 2 : 0;
                        }
                        onDoubleClicked: function (event) {
                            if (event.button === Qt.LeftButton && owner.editable)
                                browserPanel.attributesRequested();
                        }
                    }
                }
            }
            Column {
                anchors.centerIn: parent
                spacing: 12
                visible: backend.loaded && backend.count === 0
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "No objects found"
                    font.pixelSize: 16
                    color: "#b4c5d6"
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Try another ID or disable the filter."
                    color: "#7e91a5"
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 3
            Label {
                text: backend.selectedCount > 1 ? backend.selectedCount + " selected · active ID: " + (owner.d.itemId ?? "-") : backend.selected >= 0 && backend.visibleIndex() >= 0 ? "Selected ID: " + (owner.d.itemId ?? "-") : ""
                color: "#8da0b2"
                font.pixelSize: 11
            }
            Item {
                Layout.fillWidth: true
            }
            Label {
                text: backend.count + " objects"
                color: "#8da0b2"
                font.pixelSize: 11
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 4
            Tool {
                text: "↗"
                implicitWidth: 26
                implicitHeight: 24
                padding: 2
                tip: "Export object"
                enabled: backend.selected >= 0
                onClicked: owner.openObjectExport()
            }
            Tool {
                text: "▧"
                implicitWidth: 26
                implicitHeight: 24
                padding: 2
                tip: "Assign sprite"
                enabled: owner.editable
                onClicked: browserPanel.assignRequested()
            }
            Tool {
                text: "↶"
                implicitWidth: 26
                implicitHeight: 24
                padding: 2
                tip: "Undo"
                enabled: backend.canUndo
                onClicked: backend.undo()
            }
            Tool {
                text: "↷"
                implicitWidth: 26
                implicitHeight: 24
                padding: 2
                tip: "Redo"
                enabled: backend.canRedo
                onClicked: backend.redo()
            }
            Tool {
                text: "✎"
                implicitWidth: 26
                implicitHeight: 24
                padding: 2
                tip: "Item attributes"
                enabled: owner.editable
                onClicked: browserPanel.attributesRequested()
            }
            Tool {
                text: "⧉"
                implicitWidth: 26
                implicitHeight: 24
                padding: 2
                tip: "Duplicate object"
                enabled: owner.objectEditable
                onClicked: backend.create(true)
            }
            Tool {
                text: "+"
                implicitWidth: 26
                implicitHeight: 24
                padding: 2
                tip: "New object"
                enabled: backend.loaded
                onClicked: backend.create()
            }
            Tool {
                text: "×"
                implicitWidth: 26
                implicitHeight: 24
                padding: 2
                tip: "Remove selected object"
                enabled: owner.objectEditable
                onClicked: browserPanel.removeRequested()
            }
        }
    }
}
