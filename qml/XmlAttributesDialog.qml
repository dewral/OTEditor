import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: xmlDialog
    property var backend
    property var values: ({})
    property string error: ""
    title: "items.xml attributes — server ID " + backend.serverId
    width: 500
    height: 520
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.NoButton
    onOpened: refresh()
    function refresh() {
        values = backend.xmlAttributes()
        error = ""
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        Label {
            text: "Edit key/value attributes for the selected server item. Keys starting with @ are item fields such as @article. Removing a key is available for explicit item IDs; ranged entries can be overridden by adding a value."
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            ColumnLayout {
                width: xmlDialog.width - 40
                Repeater {
                    model: Object.keys(xmlDialog.values).sort()
                    delegate: RowLayout {
                        required property string modelData
                        Layout.fillWidth: true
                        Label { text: modelData; Layout.preferredWidth: 150; elide: Text.ElideRight }
                        TextField { id: editedValue; Layout.fillWidth: true; text: String(xmlDialog.values[modelData]); selectByMouse: true }
                        Button {
                            text: "Save"
                            onClicked: {
                                if (backend.setXmlAttribute(modelData,editedValue.text)) xmlDialog.refresh()
                                else xmlDialog.error=backend.status
                            }
                        }
                        Button {
                            text: "Remove"
                            onClicked: {
                                if (backend.removeXmlAttribute(modelData)) xmlDialog.refresh()
                                else xmlDialog.error=backend.status
                            }
                        }
                    }
                }
            }
        }
        Label { text: "Add attribute" }
        RowLayout {
            Layout.fillWidth: true
            TextField { id: newKey; placeholderText: "Key"; Layout.preferredWidth: 150; selectByMouse: true }
            TextField { id: newValue; placeholderText: "Value"; Layout.fillWidth: true; selectByMouse: true }
            Button {
                text: "Add"
                onClicked: {
                    if (backend.setXmlAttribute(newKey.text,newValue.text)) {
                        newKey.clear();newValue.clear();xmlDialog.refresh()
                    } else xmlDialog.error=backend.status
                }
            }
        }
        Label { text: xmlDialog.error; visible: text.length>0; color: "#ed9b9b"; wrapMode: Text.Wrap; Layout.fillWidth: true }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button { text: "Close"; onClicked: xmlDialog.close() }
        }
    }
}
