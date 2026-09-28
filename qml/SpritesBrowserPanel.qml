import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Panel {
    id: spritePanel
    property var backend
    property var owner
    signal contextMenuRequested(real x, real y)
    signal assignRequested
    signal importRequested(string mode)
    signal removeRequested

    objectName: "spritesPanel"
    visible: owner.showSpritesPanel
    title: "Sprites"
    SplitView.preferredWidth: 320
    SplitView.minimumWidth: 250
    SplitView.maximumWidth: 450
    ColumnLayout {
        anchors.fill: parent
        spacing: 7
        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            Tool {
                text: "List"
                checked: owner.spriteMode === 0
                Layout.fillWidth: true
                onClicked: owner.spriteMode = 0
            }
            Tool {
                text: "Grid"
                checked: owner.spriteMode === 1
                Layout.fillWidth: true
                onClicked: owner.spriteMode = 1
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            GridView {
                id: sprites
                anchors.fill: parent
                clip: true
                model: backend.loaded ? backend.spriteCount + 1 : 0
                cellWidth: owner.spriteMode === 0 ? width : Math.floor(width / Math.max(1, Math.floor(width / 56)))
                cellHeight: owner.spriteMode === 0 ? 39 : 58
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    id: spriteDelegate
                    required property int index
                    width: sprites.cellWidth - 1
                    height: sprites.cellHeight
                    color: owner.selectedSprite === index ? "#175886" : index % 2 === 0 ? "#272d34" : "#242a30"
                    border.color: owner.selectedSprite === index ? "#3295d2" : "#373f48"
                    Image {
                        id: spriteImage
                        property int spriteId: spriteDelegate.index
                        x: 4
                        y: 3
                        width: 32
                        height: 32
                        smooth: false
                        source: {
                            let rev = backend.revision;
                            return backend.spriteSource(index);
                        }
                        cache: false
                        Drag.active: spriteDrag.drag.active && spriteId > 0
                        Drag.dragType: Drag.Automatic
                        Drag.supportedActions: Qt.CopyAction
                        Drag.mimeData: { "application/x-oteditor-sprite-id": String(spriteId) }
                        Drag.imageSource: source
                        Drag.hotSpot: Qt.point(width / 2, height / 2)
                        Drag.onDragFinished: { x = 4; y = 3 }
                    }
                    Label {
                        text: index
                        x: owner.spriteMode === 0 ? 44 : 0
                        y: owner.spriteMode === 0 ? 11 : 37
                        width: owner.spriteMode === 0 ? 80 : parent.width
                        horizontalAlignment: owner.spriteMode === 0 ? Text.AlignLeft : Text.AlignHCenter
                        color: "#afcde4"
                        font.pixelSize: 11
                    }
                    MouseArea {
                        id: spriteDrag
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        preventStealing: true
                        drag.target: spriteImage
                        onReleased: { spriteImage.x = 4; spriteImage.y = 3 }
                        onCanceled: { spriteImage.x = 4; spriteImage.y = 3 }
                        onClicked: function (event) {
                            owner.selectedSprite = index;
                            if (event.button === Qt.RightButton) {
                                let point = mapToItem(owner.contentItem, event.x, event.y);
                                spritePanel.contextMenuRequested(point.x, point.y);
                            }
                        }
                        onDoubleClicked: {
                            owner.selectedSprite = index;
                            if (owner.editable)
                                spritePanel.assignRequested();
                        }
                    }
                }
            }
            Column {
                anchors.centerIn: parent
                spacing: 6
                visible: !backend.loaded
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "No sprites"
                    color: "#9eb0c2"
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Select an item to view its sprites."
                    color: "#71869a"
                    font.pixelSize: 11
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 5
            Tool {
                text: "|‹"
                onClicked: {
                    owner.selectedSprite = 0;
                    sprites.positionViewAtBeginning();
                }
            }
            TextField {
                text: owner.selectedSprite
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                validator: IntValidator {
                    bottom: 0
                    top: backend.spriteCount
                }
                onAccepted: {
                    owner.selectedSprite = Number(text);
                    sprites.positionViewAtIndex(owner.selectedSprite, GridView.Beginning);
                }
            }
            Tool {
                text: "›|"
                onClicked: {
                    owner.selectedSprite = backend.spriteCount;
                    sprites.positionViewAtEnd();
                }
            }
        }
        Tool {
            text: "Assign to object…"
            Layout.fillWidth: true
            enabled: owner.editable
            tip: "Double-click a sprite to assign it"
            onClicked: spritePanel.assignRequested()
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 5
            Tool {
                text: "Replace PNG…"
                Layout.fillWidth: true
                enabled: backend.loaded && owner.selectedSprite > 0
                onClicked: {
                    spritePanel.importRequested("replace");
                }
            }
            Tool {
                text: "+ PNG…"
                enabled: backend.loaded
                tip: "Add a new " + owner.spriteSize + "×" + owner.spriteSize + " sprite"
                onClicked: {
                    spritePanel.importRequested("add");
                }
            }
            Tool {
                text: "Clear"
                enabled: backend.loaded && owner.selectedSprite > 0
                tip: "Clear sprite without shifting IDs"
                onClicked: spritePanel.removeRequested()
            }
        }
    }
}
