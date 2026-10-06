import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: newAssetDialog
    property var backend
    property var owner
    objectName: "newAssetDialog"
    title: "Create Asset Files"
    anchors.centerIn: parent
    width: 410
    modal: true
    standardButtons: Dialog.NoButton
    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        GroupBox {
            title: "Version"
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                ComboBox {
                    id: newVersion
                    Layout.fillWidth: true
                    model: ["13.10", "12.00", "10.99", "10.98", "10.95", "10.94", "10.93", "10.57", "10.50", "10.10", "9.60", "8.60", "8.00", "7.80", "7.72"]
                    currentIndex: 0
                    readonly property int selectedVersion: Number(currentText.replace(".", ""))
                    popup: Popup {
                        y: newVersion.height - 1
                        width: newVersion.width
                        height: Math.min(154, contentItem.implicitHeight + topPadding + bottomPadding)
                        padding: 1
                        contentItem: ListView {
                            implicitHeight: contentHeight
                            clip: true
                            model: newVersion.delegateModel
                            currentIndex: newVersion.highlightedIndex
                            ScrollIndicator.vertical: ScrollIndicator {}
                        }
                        background: Rectangle {
                            color: "#2b333b"
                            border.color: "#737d84"
                        }
                    }
                }
            }
        }
        GroupBox {
            title: "Settings"
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                Label {
                    text: "Sprite Dimension"
                    color: "#b9c8d8"
                }
                ComboBox {
                    id: newSpriteDimension
                    Layout.preferredWidth: 165
                    model: ["32×32", "64×64", "128×128", "256×256"]
                }
            }
        }
        GroupBox {
            title: "Options"
            Layout.fillWidth: true
            GridLayout {
                anchors.fill: parent
                columns: 2
                columnSpacing: 18
                AssetToggle {
                    id: newExtended
                    text: "Extended"
                    checked: newVersion.selectedVersion >= 960
                    enabled: true
                }
                AssetToggle {
                    id: newTransparency
                    text: "Transparency"
                }
                AssetToggle {
                    id: newAnimations
                    text: "Improved animations"
                    checked: newVersion.selectedVersion >= 1050
                    enabled: true
                }
                AssetToggle {
                    id: newFrameGroups
                    text: "Frame Groups"
                    checked: newVersion.selectedVersion >= 1057
                    enabled: true
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            Item {
                Layout.fillWidth: true
            }
            Button {
                text: "Confirm"
                onClicked: {
                    newAssetDialog.close();
                    newAssetFolderDialog.open();
                }
            }
            Button {
                text: "Cancel"
                onClicked: newAssetDialog.close()
            }
        }
    }
    FolderDialog {
        id: newAssetFolderDialog
        title: "Choose a folder for new asset files"
        onAccepted: {
            if (backend.createAssetFiles(selectedFolder.toString(), newVersion.selectedVersion, newExtended.checked, newTransparency.checked, newAnimations.checked, newFrameGroups.checked, Number(newSpriteDimension.currentText.split("×")[0]))) {
                owner.frame = 0;
                owner.pattern = 0;
                owner.selectedSprite = 0;
            }
        }
    }
}
