import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: objectViewer
    property var backend
    property var owner
    objectName: "objectViewer"
    title: "Object Viewer"
    anchors.centerIn: parent
    width: 460
    height: 480
    modal: false
    standardButtons: Dialog.Close
    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            ComboBox {
                id: viewerCategory
                model: ["Items", "Outfits", "Effects", "Missiles"]
                currentIndex: backend.category
                Layout.fillWidth: true
            }
            SpinBox {
                id: viewerId
                from: viewerCategory.currentIndex === 0 ? 100 : 1
                to: 65535
                value: backend.selected >= 0 ? backend.selected + (backend.category === 0 ? 100 : 1) : 1
                editable: true
                Layout.preferredWidth: 140
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Checker {
                anchors.centerIn: parent
                width: Math.min(parent.width, parent.height)
                height: width
            }
            Image {
                anchors.centerIn: parent
                width: Math.min(parent.width, parent.height)
                height: width
                fillMode: Image.PreserveAspectFit
                smooth: false
                cache: false
                source: {
                    let revision = backend.revision;
                    return backend.previewObject(viewerCategory.currentIndex, viewerId.value, viewerFrame.value, viewerPattern.value, viewerGroup.value, viewerLayer.value);
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Frame"
            }
            SpinBox {
                id: viewerFrame
                from: 0
                to: 254
                value: 0
            }
            Label {
                text: "Pattern"
            }
            SpinBox {
                id: viewerPattern
                from: 0
                to: 255
                value: viewerCategory.currentIndex === 1 ? 2 : 0
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Group"
            }
            SpinBox {
                id: viewerGroup
                from: 0
                to: 1
                value: 0
            }
            Label {
                text: "Layer"
            }
            SpinBox {
                id: viewerLayer
                from: -1
                to: 15
                value: -1
            }
            Item {
                Layout.fillWidth: true
            }
            Button {
                text: "Show in editor"
                onClicked: {
                    backend.category = viewerCategory.currentIndex;
                    backend.jump(viewerId.value);
                    objectViewer.close();
                }
            }
        }
    }
}
