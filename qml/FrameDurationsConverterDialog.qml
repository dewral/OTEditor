import "Theme.js" as Colors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: converter
    property var backend
    objectName: "frameDurationsConverter"
    title: "Frame Durations Converter"
    anchors.centerIn: parent
    width: 440
    modal: true
    standardButtons: Dialog.Close

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label {
            text: "Enable frame durations for animated items, outfits, effects, and missiles. Static objects stay unchanged."
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Label {
            text: "Current format: " + (backend.info.durations ? "frame durations enabled" : "no frame durations")
            color: Colors.muted
        }
        RowLayout {
            Label { text: "Minimum (ms)"; Layout.preferredWidth: 110 }
            SpinBox { id: minimumDuration; from: 1; to: 60000; value: 100; editable: true }
        }
        RowLayout {
            Label { text: "Maximum (ms)"; Layout.preferredWidth: 110 }
            SpinBox { id: maximumDuration; from: 1; to: 60000; value: 100; editable: true }
        }
        Button {
            text: "Add frame durations"
            Layout.fillWidth: true
            enabled: backend.loaded && !Boolean(backend.info.durations) && minimumDuration.value <= maximumDuration.value
            onClicked: if (backend.convertFrameDurations(true,minimumDuration.value,maximumDuration.value)) converter.close()
        }
        Label {
            text: "Removing durations discards custom timing; animations keep their frames and sprites."
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            color: "#d2a879"
        }
        Button {
            text: "Remove frame durations"
            Layout.fillWidth: true
            enabled: backend.loaded && Boolean(backend.info.durations)
            onClicked: if (backend.convertFrameDurations(false)) converter.close()
        }
    }
}
