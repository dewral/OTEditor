import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: replaceDialog
    property var backend
    property var owner
    title: "Replace item " + (owner.d.itemId ?? "")
    anchors.centerIn: parent
    width: 440
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    ColumnLayout {
        anchors.fill: parent
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: "Copy graphics and properties from another item in this project. The target ID stays unchanged. You can undo this operation."
        }
        RowLayout {
            Label {
                text: "Source client ID"
            }
            SpinBox {
                id: replacementId
                from: 100
                to: 65535
                editable: true
                implicitWidth: 150
            }
        }
    }
    onAccepted: backend.replaceObject(replacementId.value)
}
