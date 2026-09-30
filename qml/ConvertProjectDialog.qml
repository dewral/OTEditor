import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: convertDialog
    property var backend
    property string error: ""
    title: "Convert client project"
    width: 500
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.NoButton
    onOpened: error = ""
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label {
            text: "Write a separate DAT/SPR project for another client version. Object IDs stay the same. Unsupported flags, frame groups and durations are adapted to the target version. The current project remains open."
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Label { text: "Target DAT version" }
        ComboBox {
            id: targetVersion
            Layout.fillWidth: true
            model: ["7.72", "7.80", "8.00", "8.60", "9.60", "10.10", "10.50", "10.57", "10.98", "12.00", "13.10"]
            currentIndex: 0
        }
        Label { text: "Output folder (must not contain Tibia.dat or Tibia.spr)" }
        RowLayout {
            Layout.fillWidth: true
            TextField { id: outputFolder; Layout.fillWidth: true; selectByMouse: true }
            Button { text: "Browse"; onClicked: folderPicker.open() }
        }
        Label { text: convertDialog.error; visible: text.length > 0; color: "#ed9b9b"; wrapMode: Text.Wrap; Layout.fillWidth: true }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button {
                text: "Convert"
                enabled: backend.loaded && outputFolder.text.length > 0
                onClicked: {
                    const version = Number(targetVersion.currentText.replace(".", ""))
                    if (backend.convertProject(outputFolder.text, version)) convertDialog.close()
                    else convertDialog.error = backend.status
                }
            }
            Button { text: "Cancel"; onClicked: convertDialog.close() }
        }
    }
    FolderDialog {
        id: folderPicker
        title: "Choose output folder"
        onAccepted: outputFolder.text = backend.localPath(selectedFolder.toString())
    }
}
