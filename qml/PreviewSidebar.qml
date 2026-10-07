import "Theme.js" as Colors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property var backend
    property var owner

    objectName: "previewPanel"
    visible: owner.showPreviewPanel
    SplitView.preferredWidth: 250
    SplitView.minimumWidth: 240
    SplitView.maximumWidth: 320
    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        Panel {
            title: "Info"
            Layout.fillWidth: true
            Layout.preferredHeight: infoRows.implicitHeight + 48
            Column {
                id: infoRows
                anchors {
                    left: parent.left
                    right: parent.right
                }
                spacing: 2
                Repeater {
                    model: [["Version", backend.info.version], ["OTB Version", backend.info.otbVersion], ["Attributes", backend.info.attributes], ["Sprite Dimension", backend.info.spriteDimension], ["Dat", backend.info.signature], ["Items", backend.info.items], ["Outfits", backend.info.outfits], ["Effects", backend.info.effects], ["Missiles", backend.info.missiles], ["Spr", backend.info.sprSignature], ["Sprites", backend.info.sprites], ["Extended", backend.info.extended ? "Yes" : "No"], ["Transparency", backend.info.alpha ? "Yes" : "No"], ["Improv. Anim.", backend.info.durations ? "Yes" : "No"], ["Frame Groups", backend.info.groups ? "Yes" : "No"], ["Metadata Controller", backend.info.metadataController]]
                    Row {
                        required property var modelData
                        width: infoRows.width
                        spacing: 8
                        height: 14
                        Label {
                            text: modelData[0] + ":"
                            width: parent.width * 0.58 - 8
                            horizontalAlignment: Text.AlignRight
                            font.pixelSize: 11
                            color: Colors.muted
                        }
                        Label {
                            text: backend.loaded ? String(modelData[1] ?? "—") : "—"
                            width: parent.width * 0.42
                            font.pixelSize: 11
                            color: Colors.accent
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
        Panel {
            title: "Preview"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 120
            ColumnLayout {
                anchors.fill: parent
                spacing: 8
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 210
                    Layout.maximumHeight: 230
                    clip: true
                    Checker {
                        anchors.fill: parent
                        anchors.margins: 4
                        visible: backend.selected < 0
                    }
                    Image {
                        objectName: "sidebarPreview"
                        anchors.centerIn: parent
                        source: owner.currentImage
                        readonly property real baseScale: Math.min(160, parent.width, parent.height) / Math.max((owner.d.itemWidth || 1) * owner.spriteSize, (owner.d.itemHeight || 1) * owner.spriteSize)
                        width: (owner.d.itemWidth || 1) * owner.spriteSize * baseScale * owner.previewZoom
                        height: (owner.d.itemHeight || 1) * owner.spriteSize * baseScale * owner.previewZoom
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        cache: false
                    }
                    Label {
                        anchors.centerIn: parent
                        text: "No item selected"
                        visible: backend.selected < 0
                        color: Colors.placeholder
                        font.pixelSize: 11
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    ComboBox {
                        Layout.fillWidth: true
                        implicitHeight: 28
                        model: ["25%", "50%", "75%", "100%", "150%", "200%", "300%", "400%"]
                        currentIndex: 3
                        onActivated: owner.previewZoom = [0.25, 0.5, 0.75, 1, 1.5, 2, 3, 4][currentIndex]
                    }
                    Tool {
                        text: "▣"
                        implicitWidth: 30
                        implicitHeight: 28
                        tip: "Fit preview"
                        onClicked: owner.previewZoom = 1
                    }
                    Tool {
                        text: "↗"
                        implicitWidth: 30
                        implicitHeight: 28
                        tip: "Export object"
                        enabled: backend.selected >= 0
                        onClicked: owner.openObjectExport()
                    }
                }
            }
        }
    }
}
