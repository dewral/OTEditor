import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: frameGroupsConverter
    property var backend
    property var owner
    objectName: "frameGroupsConverter"
    title: "Frame Groups Converter"
    anchors.centerIn: parent
    width: 440
    modal: true
    standardButtons: Dialog.Close
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label {
            text: "Split animated outfits: the first frame becomes Idle and the remaining frames become Walking. Outfits with fewer than three frames stay in one group."
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Label {
            text: "Current format: " + (backend.info.groups ? "frame groups enabled" : "single group")
            color: "#a9bdcf"
        }
        Button {
            text: "Add walking groups"
            enabled: backend.loaded && !Boolean(backend.info.groups)
            Layout.fillWidth: true
            onClicked: {
                if (backend.convertFrameGroups(true))
                    frameGroupsConverter.close();
            }
        }
        Label {
            text: "Removing groups builds a three-frame outfit from Idle and Walking frames."
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            color: "#d2a879"
        }
        CheckBox {
            id: removeMounts
            text: "Remove mounts (Pattern Z)"
            visible: Boolean(backend.info.groups) && Number(backend.info.version) >= 8.70
        }
        Button {
            text: "Remove walking groups"
            enabled: backend.loaded && Boolean(backend.info.groups)
            Layout.fillWidth: true
            onClicked: {
                if (backend.convertFrameGroups(false,removeMounts.checked))
                    frameGroupsConverter.close();
            }
        }
    }
}
