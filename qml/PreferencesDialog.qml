import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: preferencesDialog
    property var backend
    property string error: ""
    title: "Preferences"
    width: 440
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.NoButton
    onOpened: {
        const settings=backend.preferences()
        exportFolder.text=settings.exportFolder
        format.currentIndex=["png","bmp","jpg"].indexOf(settings.exportFormat)
        transparency.checked=Boolean(settings.transparentBackground)
        error=""
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label { text: "Default export folder" }
        RowLayout {
            Layout.fillWidth: true
            TextField { id: exportFolder; Layout.fillWidth: true; selectByMouse: true }
            Button { text: "Browse"; onClicked: folderPicker.open() }
        }
        RowLayout {
            Label { text: "Default format" }
            ComboBox { id: format; model: ["PNG","BMP","JPG"]; currentIndex: 0 }
        }
        CheckBox { id: transparency; text: "Transparent PNG background"; checked: true }
        Label { text: preferencesDialog.error; visible: text.length>0; color: "#ed9b9b"; wrapMode: Text.Wrap; Layout.fillWidth: true }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button {
                text: "Save"
                onClicked: {
                    if(backend.setPreferences({exportFolder:exportFolder.text,
                                               exportFormat:["png","bmp","jpg"][format.currentIndex],
                                               transparentBackground:transparency.checked}))
                        preferencesDialog.close()
                    else preferencesDialog.error="Choose an existing folder and try again."
                }
            }
            Button { text: "Cancel"; onClicked: preferencesDialog.close() }
        }
    }
    FolderDialog {
        id: folderPicker
        title: "Default export folder"
        onAccepted: exportFolder.text=backend.localPath(selectedFolder.toString())
    }
}
