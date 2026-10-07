import "Theme.js" as Colors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

CheckBox {
    id: toggleControl
    spacing: 7
    leftPadding: 0
    rightPadding: 0
    implicitHeight: 27
    opacity: enabled ? 1 : 0.45
    indicator: Rectangle {
        implicitWidth: 30
        implicitHeight: 16
        x: 0
        y: Math.round((toggleControl.height - height) / 2)
        radius: height / 2
        color: toggleControl.checked ? Colors.accent : Colors.pressed
        border.color: toggleControl.checked ? Colors.accent : "#707a81"
        border.width: 1
        Rectangle {
            width: 12
            height: 12
            radius: 6
            y: 2
            x: toggleControl.checked ? parent.width - width - 2 : 2
            color: toggleControl.checked ? "#f2f8fc" : "#aab5be"
            border.color: toggleControl.checked ? "#ffffff" : "#c4ccd2"
            Behavior on x {
                NumberAnimation {
                    duration: 110
                    easing.type: Easing.OutCubic
                }
            }
        }
    }
    contentItem: Text {
        leftPadding: toggleControl.indicator.width + toggleControl.spacing
        text: toggleControl.text
        color: Colors.text
        font: toggleControl.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
