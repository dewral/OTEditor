import "Theme.js" as Colors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: importGraphicsDialog
    property var backend
    property var owner
    objectName: "importGraphicsDialog"
    title: "Import Client Objects"
    anchors.centerIn: parent
    property string importError: ""
    function openForTarget(itemId) {
        importTargetFirst.value = Math.max(100, Math.min(65535, Number(itemId) || 100))
        importError = ""
        open()
    }
    width: 600
    modal: true
    standardButtons: Dialog.NoButton
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label {
            text: "Target item: " + (owner.d.itemId ?? "-")
            font.bold: true
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: "Import DAT objects and their sprites. Source IDs map in order to target IDs. Missing target IDs are created automatically; unsupported flags from newer clients are omitted. Server IDs and names come from the selected Server Items Folder and are not changed by this import."
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: importGraphicsFolder
                Layout.fillWidth: true
                placeholderText: "Source client folder"
            }
            Button {
                text: "Browse"
                onClicked: importGraphicsFolderPicker.open()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Version:"
            }
            SpinBox {
                id: importGraphicsVersion
                from: 0
                to: 1310
                value: 0
                editable: true
                implicitWidth: 155
            }
            Label {
                text: importGraphicsVersion.value === 0 ? "Auto detect" : "e.g. 1098 = 10.98"
                color: Colors.muted
            }
        }
        RowLayout {
            Label {
                text: "Source IDs:"
            }
            SpinBox {
                id: importSourceFirst
                from: 100
                to: 65535
                value: 5090
                editable: true
                implicitWidth: 180
            }
            Label {
                text: "to"
            }
            SpinBox {
                id: importSourceLast
                from: 100
                to: 65535
                value: 5090
                editable: true
                implicitWidth: 180
            }
        }
        RowLayout {
            Label {
                text: "Target starts at:"
            }
            SpinBox {
                id: importTargetFirst
                objectName: "importTargetFirstSpinBox"
                from: 100
                to: 65535
                value: 100
                editable: true
                implicitWidth: 180
            }
            Label {
                text: "ends at " + (importTargetFirst.value + importSourceLast.value - importSourceFirst.value)
                color: Colors.muted
            }
        }
        Label {
            visible: importGraphicsDialog.importError.length > 0
            text: importGraphicsDialog.importError
            color: "#f0a0a0"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button {
                text: "Cancel"
                onClicked: importGraphicsDialog.close()
            }
            Button {
                text: "Import"
                enabled: importGraphicsFolder.text.length > 0 && importSourceLast.value >= importSourceFirst.value && importTargetFirst.value + importSourceLast.value - importSourceFirst.value <= 65535
                onClicked: {
                    importGraphicsDialog.importError = "";
                    if (backend.importItemGraphicsRange(importGraphicsFolder.text, importGraphicsVersion.value, importSourceFirst.value, importSourceLast.value, importTargetFirst.value) > 0)
                        importGraphicsDialog.close();
                    else
                        importGraphicsDialog.importError = backend.status;
                }
            }
        }
    }
    FolderDialog {
        id: importGraphicsFolderPicker
        title: "Select source client folder"
        onAccepted: {
            importGraphicsFolder.text = backend.localPath(selectedFolder.toString());
            importGraphicsVersion.value = backend.detectFolderVersion(importGraphicsFolder.text);
        }
    }
}
