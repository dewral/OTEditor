import "Theme.js" as Colors
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
        color: root.accent ? (root.down ? "#2787cb" : Colors.accent)
                          : root.checked ? (root.hovered ? Colors.selectedHover : Colors.selected)
                          : root.down ? Colors.pressed : root.hovered ? Colors.hover : Colors.button
        border.color: root.accent ? "transparent" : Colors.border
        opacity: root.enabled ? 1 : 0.4
        Rectangle {
            visible: root.checked && !root.accent
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 12
            height: 3
            radius: 1.5
            color: Colors.accent
        }
    }
    contentItem: Text {
        text: root.text
        font: root.font
        color: root.enabled ? (root.accent ? "#101b23" : Colors.text) : Colors.disabled
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    ToolTip.visible: hovered && tip.length > 0
    ToolTip.text: tip
    ToolTip.delay: 650
}
