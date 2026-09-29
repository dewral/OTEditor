import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: mergeDialog
    property var backend
    property string error: ""
    title: "Merge client project"
    width: 500
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.NoButton
    onOpened: error=""
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label {
            text: "Append every source item, outfit, effect and missile with its sprites. The projects must have matching client versions and DAT/SPR options. Existing IDs stay unchanged; OTB entries are not merged."
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Label { text: "Source client folder" }
        RowLayout {
            Layout.fillWidth: true
            TextField { id: sourceFolder; Layout.fillWidth: true; selectByMouse: true }
            Button { text: "Browse"; onClicked: folderPicker.open() }
        }
        RowLayout {
            Label { text: "Client version (0 = auto detect)" }
            SpinBox { id: sourceVersion; from: 0; to: 9999; editable: true; value: 0 }
        }
        Label { text: mergeDialog.error; visible: text.length>0; color: "#ed9b9b"; wrapMode: Text.Wrap; Layout.fillWidth: true }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button {
                text: "Merge"
                enabled: sourceFolder.text.length>0 && backend.loaded
                onClicked: {
                    const count=backend.mergeProject(sourceFolder.text,sourceVersion.value)
                    if(count>0)mergeDialog.close()
                    else mergeDialog.error=backend.status
                }
            }
            Button { text: "Cancel"; onClicked: mergeDialog.close() }
        }
    }
    FolderDialog {
        id: folderPicker
        title: "Choose source client folder"
        onAccepted: {
            sourceFolder.text=backend.localPath(selectedFolder.toString())
            sourceVersion.value=Math.max(0,backend.detectFolderVersion(sourceFolder.text))
        }
    }
}
