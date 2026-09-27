import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: animationDialog
    property var backend
    property var owner
    objectName: "animationDialog"
    title: "Animation Editor"
    anchors.centerIn: parent
    width: 430
    modal: true
    standardButtons: Dialog.Close
    readonly property int objectId: Number(owner.d.itemId || 0)
    readonly property var duration: {
        let revision = backend.revision;
        return backend.frameDuration(backend.category, objectId, animationGroup.value, animationFrame.value);
    }
    function refreshDuration() {
        let data = backend.frameDuration(backend.category, objectId, animationGroup.value, animationFrame.value);
        animationMinimum.value = Number(data.minimum || 100);
        animationMaximum.value = Number(data.maximum || 100);
    }
    onOpened: refreshDuration()
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label {
            text: "Object " + animationDialog.objectId
            font.bold: true
        }
        RowLayout {
            Label {
                text: "Frame group"
            }
            SpinBox {
                id: animationGroup
                from: 0
                to: Math.max(0, Number(owner.d.frameGroupCount || 1) - 1)
                value: 0
                onValueModified: {
                    animationFrame.value = 0;
                    animationDialog.refreshDuration();
                }
            }
        }
        RowLayout {
            Label {
                text: "Frame"
            }
            SpinBox {
                id: animationFrame
                from: 0
                to: {
                    let revision = backend.revision;
                    return Math.max(0, Number(backend.frameDuration(backend.category, animationDialog.objectId, animationGroup.value, 0).frames || 1) - 1);
                }
                value: 0
                onValueModified: animationDialog.refreshDuration()
            }
        }
        RowLayout {
            Label {
                text: "Minimum (ms)"
            }
            SpinBox {
                id: animationMinimum
                from: 1
                to: 60000
                value: 100
                editable: true
            }
        }
        RowLayout {
            Label {
                text: "Maximum (ms)"
            }
            SpinBox {
                id: animationMaximum
                from: 1
                to: 60000
                value: 100
                editable: true
            }
        }
        Button {
            text: "Apply to frame"
            Layout.alignment: Qt.AlignRight
            enabled: Number(animationDialog.duration.frames || 1) > 1 && animationMinimum.value <= animationMaximum.value
            onClicked: backend.setFrameDuration(backend.category, animationDialog.objectId, animationGroup.value, animationFrame.value, animationMinimum.value, animationMaximum.value)
        }
        RowLayout {
            Layout.fillWidth: true
            Button {
                text: "Duplicate frame"
                enabled: Number(animationDialog.duration.frames || 1) < 255
                onClicked: backend.duplicateFrame(backend.category, animationDialog.objectId, animationGroup.value, animationFrame.value)
            }
            Button {
                text: "Delete frame"
                enabled: Number(animationDialog.duration.frames || 1) > 1
                onClicked: {
                    if (backend.deleteFrame(backend.category, animationDialog.objectId, animationGroup.value, animationFrame.value))
                        animationFrame.value = Math.max(0, animationFrame.value - 1);
                }
            }
        }
    }
}
