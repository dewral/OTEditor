import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Panel {
    property var backend
    property var owner

    title: "›  Log"
    Layout.fillWidth: true
    Layout.preferredHeight: 102
    visible: owner.showLog
    ScrollView {
        anchors {
            fill: parent
            rightMargin: 75
        }
        clip: true
        TextArea {
            text: backend.log
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
            font.family: "Consolas"
            font.pixelSize: 11
            color: "#9fc5e4"
            background: null
        }
    }
    Tool {
        anchors {
            right: parent.right
            top: parent.top
        }
        text: "Clear"
        implicitWidth: 65
        onClicked: backend.clearLog()
    }
}
