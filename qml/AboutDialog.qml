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
            text: "OTEditor " + (Qt.application.version || "0.1.1") + "\nQt Quick / QML · C++ · otformats\n\nEdit DAT objects and SPR sprites, including items, outfits, effects and missiles.\n\nSprite sheets, pixel painting, animation tools, Slicer and AI Sprite Generator. Optional items.otb and items.xml server data."
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
