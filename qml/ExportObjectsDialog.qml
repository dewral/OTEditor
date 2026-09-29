import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: objectExportDialog
    property var backend
    property var owner
    objectName: "objectExportDialog"
    title: "Export"
    anchors.centerIn: parent
    width: 420
    modal: true
    standardButtons: Dialog.NoButton
    property string exportError: ""
    onOpened: {
        const categories = ["item", "outfit", "effect", "missile"];
        exportName.text = categories[backend.category];
        const preferences=backend.preferences();
        exportFolder.text = preferences.exportFolder;
        formatPng.checked = preferences.exportFormat === "png";
        formatBmp.checked = preferences.exportFormat === "bmp";
        formatJpg.checked = preferences.exportFormat === "jpg";
        transparentExport.checked = Boolean(preferences.transparentBackground);
        exportError = "";
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 9
        Label {
            text: "Name:"
            color: "#c9d8e8"
        }
        TextField {
            id: exportName
            objectName: "exportNameField"
            Layout.fillWidth: true
            selectByMouse: true
        }
        Label {
            text: "Output Folder:"
            color: "#c9d8e8"
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            TextField {
                id: exportFolder
                objectName: "exportFolderField"
                Layout.fillWidth: true
                selectByMouse: true
            }
            Button {
                text: "Browse"
                onClicked: objectExportFolderDialog.open()
            }
        }
        GroupBox {
            title: "Format"
            Layout.fillWidth: true
            RowLayout {
                anchors.fill: parent
                spacing: 12
                RadioButton {
                    id: formatPng
                    objectName: "exportPngFormat"
                    text: "PNG"
                    checked: true
                }
                RadioButton {
                    id: formatBmp
                    objectName: "exportBmpFormat"
                    text: "BMP"
                }
                RadioButton {
                    id: formatJpg
                    objectName: "exportJpgFormat"
                    text: "JPG"
                }
                RadioButton {
                    id: formatObd
                    text: "OBD"
                    enabled: Number(String(backend.info.spriteDimension || "32x32").split("x")[0]) === 32
                }
            }
        }
        GroupBox {
            title: "Options"
            Layout.fillWidth: true
            AssetToggle {
                id: transparentExport
                objectName: "transparentExportToggle"
                text: "Transparent background"
                checked: true
                enabled: formatPng.checked
            }
        }
        Label {
            text: objectExportDialog.exportError
            visible: text.length > 0
            color: "#ed9b9b"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
        }
        RowLayout {
            Layout.fillWidth: true
            Item {
                Layout.fillWidth: true
            }
            Button {
                objectName: "confirmObjectExport"
                text: "Confirm"
                onClicked: {
                    const format = formatPng.checked ? "png" : formatBmp.checked ? "bmp" : formatJpg.checked ? "jpg" : "obd";
                    const count = backend.exportObjects(exportFolder.text, exportName.text, format, transparentExport.checked);
                    if (count > 0)
                        objectExportDialog.close();
                    else
                        objectExportDialog.exportError = backend.status;
                }
            }
            Button {
                text: "Cancel"
                onClicked: objectExportDialog.close()
            }
        }
    }
    FolderDialog {
        id: objectExportFolderDialog
        title: "Choose output folder"
        onAccepted: exportFolder.text = backend.localPath(selectedFolder.toString())
    }
}
