import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: about
    objectName: "aboutDialog"
    title: "About OTEditor"
    anchors.centerIn: parent
    width: 490
    modal: true
    standardButtons: Dialog.Close
    ColumnLayout {
        width: parent.width
        spacing: 12
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: "OTEditor 0.1\nQt Quick / QML · C++ · otformats\n\nClient object browser and item editor.\nDAT item editing, sprite assignment, undo/redo and PNG export.\n\nOutfits, effects and missiles: preview and export.\nPixel painting and full ObjectBuilder feature parity are not included in this version."
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: "#46515c"
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "Support server"
            color: "#9aabba"
            font.pixelSize: 11
        }
        Image {
            objectName: "midhemSupportLogo"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 260
            Layout.preferredHeight: 90
            source: "qrc:/assets/Midhem.png"
            fillMode: Image.PreserveAspectFit
            smooth: true
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "Midhem Online"
            font.pixelSize: 15
            font.bold: true
        }
    }
}
