import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: spriteOptimizer
    property var backend
    property var owner
    objectName: "spriteOptimizer"
    title: "Sprites Optimizer"
    anchors.centerIn: parent
    width: 440
    modal: true
    standardButtons: Dialog.NoButton
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label {
            text: "Find duplicate sprites, redirect object references and clear unused sprite data."
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        CheckBox {
            id: compactSpriteIds
            text: "Compact sprite IDs"
            checked: false
        }
        Label {
            text: compactSpriteIds.checked ? "Sprite IDs will change; external references may need updating." : "Sprite IDs stay unchanged. Compile to save the optimized SPR."
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            color: "#a9bdcf"
        }
        RowLayout {
            Layout.fillWidth: true
            Item {
                Layout.fillWidth: true
            }
            Button {
                text: "Optimize"
                onClicked: {
                    spriteOptimizer.close();
                    backend.optimizeSprites(compactSpriteIds.checked);
                }
            }
            Button {
                text: "Cancel"
                onClicked: spriteOptimizer.close()
            }
        }
    }
}
