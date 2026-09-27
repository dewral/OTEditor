import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: lookTypeDialog
    property var backend
    property var owner
    objectName: "lookTypeDialog"
    title: "LookType Generator"
    anchors.centerIn: parent
    width: 430
    modal: true
    standardButtons: Dialog.Close
    readonly property string xml: {
        let attributes = lookKind.currentIndex === 0 ? 'type="' + lookId.value + '"' : 'typeex="' + lookId.value + '"';
        for (let entry of [["head", lookHead], ["body", lookBody], ["legs", lookLegs], ["feet", lookFeet], ["addons", lookAddons], ["mount", lookMount], ["corpse", lookCorpse]])
            if (entry[1].value > 0)
                attributes += ' ' + entry[0] + '="' + entry[1].value + '"';
        return '<look ' + attributes + '/>';
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        RowLayout {
            ComboBox {
                id: lookKind
                model: ["Outfit", "Item"]
                Layout.fillWidth: true
            }
            SpinBox {
                id: lookId
                from: 1
                to: 65535
                value: backend.category === 1 ? (owner.d.itemId || 1) : 1
                editable: true
                Layout.preferredWidth: 130
            }
        }
        GridLayout {
            columns: 2
            columnSpacing: 14
            rowSpacing: 5
            Label {
                text: "Head"
            }
            SpinBox {
                id: lookHead
                from: 0
                to: 255
                editable: true
            }
            Label {
                text: "Body"
            }
            SpinBox {
                id: lookBody
                from: 0
                to: 255
                editable: true
            }
            Label {
                text: "Legs"
            }
            SpinBox {
                id: lookLegs
                from: 0
                to: 255
                editable: true
            }
            Label {
                text: "Feet"
            }
            SpinBox {
                id: lookFeet
                from: 0
                to: 255
                editable: true
            }
            Label {
                text: "Addons"
            }
            SpinBox {
                id: lookAddons
                from: 0
                to: 255
                editable: true
            }
            Label {
                text: "Mount"
            }
            SpinBox {
                id: lookMount
                from: 0
                to: 65535
                editable: true
            }
            Label {
                text: "Corpse"
            }
            SpinBox {
                id: lookCorpse
                from: 0
                to: 65535
                editable: true
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 130
            Checker {
                anchors.centerIn: parent
                width: 128
                height: 128
            }
            Image {
                anchors.centerIn: parent
                width: 128
                height: 128
                fillMode: Image.PreserveAspectFit
                smooth: false
                cache: false
                source: {
                    let revision = backend.revision;
                    return backend.previewObject(lookKind.currentIndex === 0 ? 1 : 0, lookId.value, 0, lookKind.currentIndex === 0 ? 2 : 0);
                }
            }
        }
        TextField {
            text: lookTypeDialog.xml
            readOnly: true
            selectByMouse: true
            Layout.fillWidth: true
        }
        Button {
            text: "Copy look XML"
            Layout.alignment: Qt.AlignRight
            onClicked: backend.copyText(lookTypeDialog.xml)
        }
    }
}
