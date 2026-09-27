import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: durationOptimizer
    property var backend
    property var owner
    objectName: "durationOptimizer"
    title: "Frame Durations Optimizer"
    anchors.centerIn: parent
    width: 410
    modal: true
    standardButtons: Dialog.Close
    ColumnLayout {
        anchors.fill: parent
        spacing: 9
        Label {
            text: "Set a duration range for every animation frame in selected categories."
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        CheckBox {
            id: durationItems
            text: "Items"
            checked: true
        }
        CheckBox {
            id: durationOutfits
            text: "Outfits"
            checked: true
        }
        CheckBox {
            id: durationEffects
            text: "Effects"
            checked: true
        }
        RowLayout {
            Label {
                text: "Minimum (ms)"
            }
            SpinBox {
                id: durationMin
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
                id: durationMax
                from: 1
                to: 60000
                value: 100
                editable: true
            }
        }
        Button {
            text: "Optimize durations"
            Layout.alignment: Qt.AlignRight
            enabled: durationMin.value <= durationMax.value && (durationItems.checked || durationOutfits.checked || durationEffects.checked)
            onClicked: {
                backend.optimizeFrameDurations(durationItems.checked, durationOutfits.checked, durationEffects.checked, durationMin.value, durationMax.value);
                durationOptimizer.close();
            }
        }
    }
}
