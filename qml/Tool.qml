import QtQuick
import QtQuick.Controls
Button {
    id: root
    property string tip: ""
    property bool accent: false
    implicitWidth: Math.max(32, contentItem.implicitWidth + 22); implicitHeight: 30
    hoverEnabled: true; padding: 6; font.pixelSize: 12
    background: Rectangle {
        radius: 2
        border.color: root.accent || root.checked ? "#329bdd" : root.hovered ? "#677687" : "#46505b"
        gradient: Gradient {
            GradientStop { position: 0; color: root.accent || root.checked ? "#1775b5" : root.down ? "#20252b" : root.hovered ? "#3e4854" : "#353c45" }
            GradientStop { position: 1; color: root.accent || root.checked ? "#12578c" : "#2b3139" }
        }
        opacity: root.enabled ? 1 : 0.4
    }
    contentItem: Text { text: root.text; font: root.font; color: root.enabled ? "#e0e9f3" : "#77828e"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
    ToolTip.visible: hovered && tip.length > 0; ToolTip.text: tip; ToolTip.delay: 650
}

