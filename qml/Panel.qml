import QtQuick
import QtQuick.Controls
Rectangle {
    id: root
    property string title: ""
    default property alias contents: body.data
    color: "#24292f"; border.color: "#3b424b"; radius: 2
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 } height: 28
        gradient: Gradient { GradientStop { position: 0; color: "#30363e" } GradientStop { position: 1; color: "#282e35" } }
        Label { anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter } text: root.title; font.bold: true; color: "#e5eaf1" }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#3b424b" }
    }
    Item { id: body; anchors { fill: parent; topMargin: 36; leftMargin: 10; rightMargin: 10; bottomMargin: 10 } }
}

