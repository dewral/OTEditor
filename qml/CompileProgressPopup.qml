import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Popup {
    id: compilePopup
    property var backend
    property var owner
    objectName: "compileProgressPopup"
    parent: owner.contentItem
    anchors.centerIn: parent
    width: 400
    padding: 20
    modal: true
    closePolicy: Popup.NoAutoClose
    visible: backend.compiling
    background: Rectangle {
        color: "#252c33"
        border.color: "#536171"
        radius: 4
    }
    ColumnLayout {
        width: parent.width
        spacing: 12
        Label {
            text: backend.compileStage.startsWith("Import:") ? "Importing objects" : backend.compileStage.startsWith("Creating OTB items") ? "Creating OTB items" : backend.compileStage.startsWith("Exporting") ? "Exporting selected objects" : backend.compileStage === "Optimizing sprites" ? "Optimizing sprites" : "Compiling project"
            font.bold: true
            font.pixelSize: 15
            color: "#dce5ee"
        }
        Label {
            text: backend.compileStage
            color: "#a9bdcf"
        }
        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: 100
            value: backend.compileProgress
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: backend.compileProgress + "%"
            color: "#a9bdcf"
        }
    }
}
