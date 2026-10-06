import QtQuick
import QtQuick.Controls
Button {
    id: root
    property string tip: ""
    property bool accent: false
    implicitWidth: Math.max(32, contentItem.implicitWidth + 22)
    implicitHeight: 32
    hoverEnabled: true
    padding: 7
    font.pixelSize: 12
    background: Rectangle {
        radius: 4
        color: root.accent ? (root.down ? "#2787cb" : "#399ee8")
                          : root.down ? "#363d41" : root.hovered ? "#30373b" : "#282d30"
        border.color: root.accent ? "transparent" : "#343a3e"
        opacity: root.enabled ? 1 : 0.4
        Rectangle {
            visible: root.checked && !root.accent
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 12
            height: 3
            radius: 1.5
            color: "#55b4f4"
        }
    }
    contentItem: Text {
        text: root.text
        font: root.font
        color: root.enabled ? (root.accent ? "#101b23" : "#edf0f2") : "#7d8991"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    ToolTip.visible: hovered && tip.length > 0
    ToolTip.text: tip
    ToolTip.delay: 650
}
