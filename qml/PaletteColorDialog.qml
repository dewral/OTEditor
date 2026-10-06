import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: picker
    property var owner
    property string field: ""
    property int selectedIndex: 0
    title: field === "lightColor" ? "Light Color" : "Automap Color"
    width: 420
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    onAccepted: owner.edit(field, selectedIndex)

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label {
            text: "Choose a client palette color (0–215):"
            color: "#c7d8e8"
        }
        GridView {
            id: paletteGrid
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 360
            Layout.preferredHeight: 240
            cellWidth: 20
            cellHeight: 20
            model: 216
            interactive: false
            clip: true
            delegate: Rectangle {
                width: 18
                height: 18
                color: owner.paletteColor(index)
                border.width: picker.selectedIndex === index ? 2 : 1
                border.color: picker.selectedIndex === index ? "#ffffff" : "#707a81"
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: picker.selectedIndex = index
                }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8
            Rectangle {
                width: 24
                height: 20
                color: owner.paletteColor(picker.selectedIndex)
                border.color: "#8e9ba4"
            }
            Label {
                text: "Color index: " + picker.selectedIndex
                color: "#c7d8e8"
            }
        }
    }
}
