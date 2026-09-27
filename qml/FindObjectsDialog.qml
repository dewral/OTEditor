import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: findDialog
    property var backend
    property var owner
    objectName: "findObjectsDialog"
    title: "Find objects"
    anchors.centerIn: parent
    width: 360
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    property string previousQuery: ""
    onOpened: {
        previousQuery = owner.objectSearchQuery;
        findQuery.text = previousQuery;
        findQuery.forceActiveFocus();
        findQuery.selectAll();
    }
    onRejected: {
        owner.objectSearchQuery = previousQuery;
        owner.refreshFilter();
    }
    ColumnLayout {
        anchors.fill: parent
        Label {
            text: "Search client ID:"
        }
        TextField {
            id: findQuery
            objectName: "searchField"
            Layout.fillWidth: true
            placeholderText: "Enter all or part of an ID"
            selectByMouse: true
            onTextChanged: if (findDialog.visible) {
                owner.objectSearchQuery = text.trim();
                owner.refreshFilter();
            }
            onAccepted: findDialog.accept()
        }
    }
}
