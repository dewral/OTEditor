import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: pixelEditor
    property var backend
    property color paintColor: "#ffffff"
    property bool erase: false
    title: backend.pixelEditSpriteId > 0 ? "Edit sprite " + backend.pixelEditSpriteId : "Paint new sprite"
    width: 590
    height: 670
    modal: true
    standardButtons: Dialog.Save | Dialog.Cancel
    onAccepted: backend.savePixelEdit()
    onRejected: backend.cancelPixelEdit()
    function paintAt(x,y) {
        const size=backend.pixelEditSize
        if(size<1)return
        backend.paintPixel(Math.floor(x/pixelImage.width*size),Math.floor(y/pixelImage.height*size),
                           erase ? Qt.rgba(0,0,0,0) : paintColor)
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        RowLayout {
            Button {
                text: "Color"
                onClicked: colorPicker.open()
            }
            Rectangle { width: 28; height: 28; color: pixelEditor.paintColor; border.color: "#738394" }
            CheckBox { text: "Eraser"; checked: pixelEditor.erase; onClicked: pixelEditor.erase=checked }
            Item { Layout.fillWidth: true }
            Button { text: "Reset pixels"; onClicked: backend.resetPixelEdit() }
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Checker { anchors.centerIn: parent; width: pixelImage.width; height: pixelImage.height }
            Image {
                id: pixelImage
                anchors.centerIn: parent
                width: Math.min(500,backend.pixelEditSize*12)
                height: width
                source: backend.pixelEditActive ? "image://itempreview/pixel-editor?v="+backend.pixelEditRevision : ""
                cache: false
                smooth: false
                fillMode: Image.Stretch
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.CrossCursor
                    onPressed: function(mouse) { pixelEditor.paintAt(mouse.x,mouse.y) }
                    onPositionChanged: function(mouse) {
                        if(mouse.buttons & Qt.LeftButton) pixelEditor.paintAt(mouse.x,mouse.y)
                    }
                }
            }
        }
        Label {
            text: backend.pixelEditSpriteId > 0
                  ? "Editing this sprite also changes other objects that reference its ID."
                  : "Saving adds a new sprite and assigns it to this cell."
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }
    ColorDialog {
        id: colorPicker
        title: "Paint color"
        selectedColor: pixelEditor.paintColor
        onAccepted: pixelEditor.paintColor=selectedColor
    }
}
