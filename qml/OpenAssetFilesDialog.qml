import "Theme.js" as Colors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: loadDialog
    property var backend
    property var owner
    objectName: "loadDialog"
    title: "Open Asset Files"
    anchors.centerIn: parent
    width: 510
    modal: true
    standardButtons: Dialog.NoButton
    property var previewData: ({})
    property int dimensionOverride: 0
    function autoDetectFolder() {
        dimensionOverride = 0;
        let detected = backend.detectFolderVersion(folderPath.text);
        if (detected) {
            let index = loadVersion.model.findIndex(label => Number(label.replace(".", "")) === detected);
            if (index >= 0)
                loadVersion.currentIndex = index;
        }
        refreshPreview();
    }
    function refreshPreview() {
        previewData = backend.inspectFolder(folderPath.text, loadVersion.selectedVersion, loadTransparency.checked, serverFolderPath.text, dimensionOverride);
        if (previewData.ok) {
            let sizes = [32, 64, 128, 256];
            loadDimension.currentIndex = Math.max(0, sizes.indexOf(Number(previewData.spriteSize)));
            loadTransparency.checked = Boolean(previewData.transparency);
        }
    }
    onOpened: refreshPreview()
    ColumnLayout {
        anchors.fill: parent
        spacing: 9
        Label {
            text: "Client Folder:"
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: folderPath
                Layout.fillWidth: true
                placeholderText: "Folder with Tibia.dat and Tibia.spr"
                selectByMouse: true
                onEditingFinished: loadDialog.autoDetectFolder()
            }
            Button {
                text: "Browse"
                onClicked: folderDialog.open()
            }
        }
        Label {
            text: "Server Files Folder (optional):"
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: serverFolderPath
                objectName: "serverFilesFolder"
                Layout.fillWidth: true
                placeholderText: "Folder containing items.otb and items.xml"
                selectByMouse: true
                onEditingFinished: loadDialog.refreshPreview()
            }
            Button {
                text: "Browse"
                onClicked: serverFolderDialog.open()
            }
            Button {
                text: "Clear"
                enabled: serverFolderPath.text.length > 0
                onClicked: { serverFolderPath.text = ""; loadDialog.refreshPreview() }
            }
        }
        Label {
            text: serverFolderPath.text.length ? serverFolderPath.text : "Client files only — server attributes will be hidden"
            color: Colors.muted
            font.pixelSize: 11
            elide: Text.ElideMiddle
            Layout.fillWidth: true
        }
        Label {
            text: "items.otb: " + (loadDialog.previewData.otb ? "found" : "not found")
                  + " · items.xml: " + (loadDialog.previewData.itemsXml ? "found" : "not found")
            visible: Boolean(loadDialog.previewData.ok) && serverFolderPath.text.length > 0
            color: loadDialog.previewData.otb || loadDialog.previewData.itemsXml ? "#91c8a7" : "#d5ae79"
            elide: Text.ElideMiddle
            Layout.fillWidth: true
        }
        GroupBox {
            title: "Version"
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                ComboBox {
                    id: loadVersion
                    Layout.fillWidth: true
                    model: ["13.10", "12.00", "10.99", "10.98", "10.95", "10.94", "10.93", "10.57", "10.50", "10.10", "9.60", "8.60", "8.00", "7.80", "7.72"]
                    currentIndex: 3
                    readonly property int selectedVersion: Number(currentText.replace(".", ""))
                    onActivated: loadDialog.refreshPreview()
                    popup: Popup {
                        y: loadVersion.height - 1
                        width: loadVersion.width
                        height: Math.min(154, contentItem.implicitHeight + topPadding + bottomPadding)
                        padding: 1
                        contentItem: ListView {
                            implicitHeight: contentHeight
                            clip: true
                            model: loadVersion.delegateModel
                            currentIndex: loadVersion.highlightedIndex
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
            GridLayout {
                anchors.fill: parent
                columns: 2
                columnSpacing: 12
                Label {
                    text: "Sprite Dimension"
                }
                Label {
                    text: "Attribute Server:"
                    visible: Boolean(loadDialog.previewData.otb)
                }
                ComboBox {
                    id: loadDimension
                    Layout.fillWidth: true
                    model: ["32×32", "64×64", "128×128", "256×256"]
                    onActivated: {
                        loadDialog.dimensionOverride = Number(currentText.split("×")[0]);
                        loadDialog.refreshPreview();
                    }
                }
                ComboBox {
                    Layout.fillWidth: true
                    model: [loadDialog.previewData.attributeServer || "TFS 1.4"]
                    visible: Boolean(loadDialog.previewData.otb)
                    enabled: false
                }
            }
        }
        GroupBox {
            title: "Options"
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                Label {
                    text: "Extended sprites, frame durations and frame groups are read from OTFI or the selected DAT version. Change them with the converters after opening."
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                    color: "#9db2c5"
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 18
                AssetToggle {
                    text: "Extended"
                    checked: Boolean(loadDialog.previewData.extended)
                    enabled: false
                }
                AssetToggle {
                    id: loadTransparency
                    text: "Transparency"
                    checked: Boolean(loadDialog.previewData.transparency)
                    enabled: !loadDialog.previewData.hasOtfi
                    onClicked: loadDialog.refreshPreview()
                }
                AssetToggle {
                    text: "Improved animations"
                    checked: Boolean(loadDialog.previewData.durations)
                    enabled: false
                }
                AssetToggle {
                    text: "Frame Groups"
                    checked: Boolean(loadDialog.previewData.groups)
                    enabled: false
                }
                }
            }
        }
        GroupBox {
            title: "DAT"
            Layout.fillWidth: true
            GridLayout {
                anchors.fill: parent
                columns: 2
                columnSpacing: 10
                Label {
                    text: "Signature:"
                }
                Label {
                    text: loadDialog.previewData.datSignature || "—"
                    color: Colors.accent
                }
                Label {
                    text: "Items:"
                }
                Label {
                    text: loadDialog.previewData.items ?? "—"
                    color: Colors.accent
                }
                Label {
                    text: "Outfits:"
                }
                Label {
                    text: loadDialog.previewData.outfits ?? "—"
                    color: Colors.accent
                }
                Label {
                    text: "Effects:"
                }
                Label {
                    text: loadDialog.previewData.effects ?? "—"
                    color: Colors.accent
                }
                Label {
                    text: "Missiles:"
                }
                Label {
                    text: loadDialog.previewData.missiles ?? "—"
                    color: Colors.accent
                }
            }
        }
        GroupBox {
            title: "SPR"
            Layout.fillWidth: true
            GridLayout {
                anchors.fill: parent
                columns: 2
                columnSpacing: 10
                Label {
                    text: "Signature:"
                }
                Label {
                    text: loadDialog.previewData.sprSignature || "—"
                    color: Colors.accent
                }
                Label {
                    text: "Sprites:"
                }
                Label {
                    text: loadDialog.previewData.sprites ?? "—"
                    color: Colors.accent
                }
            }
        }
        Label {
            text: loadDialog.previewData.error || ""
            visible: text.length > 0
            color: "#e9a7a7"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 5
            Item {
                Layout.fillWidth: true
            }
            Button {
                text: "Load"
                enabled: Boolean(loadDialog.previewData.ok)
                onClicked: {
                    if (backend.openFolder(folderPath.text, loadVersion.selectedVersion, loadTransparency.checked, serverFolderPath.text, loadDialog.dimensionOverride)) {
                        owner.frame = 0;
                        owner.pattern = 0;
                        owner.selectedSprite = 0;
                        loadDialog.close();
                    }
                }
            }
            Button {
                text: "Cancel"
                onClicked: loadDialog.close()
            }
        }
    }
    FolderDialog {
        id: folderDialog
        title: "Select client folder"
        onAccepted: {
            folderPath.text = backend.localPath(selectedFolder.toString());
            loadDialog.autoDetectFolder();
        }
    }
    FolderDialog {
        id: serverFolderDialog
        title: "Select folder containing items.otb and items.xml"
        onAccepted: {
            serverFolderPath.text = backend.localPath(selectedFolder.toString());
            loadDialog.refreshPreview();
        }
    }
}
