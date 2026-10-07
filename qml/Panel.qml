import "Theme.js" as Colors
import QtQuick
import QtQuick.Controls
Rectangle {
    id: root
    property string title: ""
    default property alias contents: body.data
    color: Colors.surface; border.color: Colors.border; radius: 6
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 } height: 30
        color: "transparent"
        Label { anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter } text: root.title; font.bold: true; color: Colors.text }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Colors.border }
    }
    Item { id: body; anchors { fill: parent; topMargin: 36; leftMargin: 10; rightMargin: 10; bottomMargin: 10 } }
}

