import "Theme.js" as Colors
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: compileDialog
    objectName: "compileAssetFilesDialog"
    property var backend
    property string error: ""
    readonly property var versions: {
        const choices = ["7.72", "7.80", "8.00", "8.60", "9.60", "10.10", "10.50", "10.57", "10.93", "10.94", "10.95", "10.98", "10.99", "12.00", "13.10"]
        const current = backend && backend.loaded ? String(backend.info.version) : ""
        if (current && choices.indexOf(current) < 0) choices.push(current)
        return choices
    }
    title: "Compile Asset Files"
    width: 510
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.NoButton

    function setVersionDefaults() {
        const version = targetVersion.selectedVersion
        extended.checked = version >= 960
        animations.checked = version >= 1050
        frameGroups.checked = version >= 1057
    }

    onOpened: {
        error = ""
        const info = backend.info
        nameField.text = String(info.dat || "Tibia.dat").replace(/\.dat$/i, "")
        outputFolder.text = ""
        const sourceVersion = String(info.version || "10.98")
        let index = compileDialog.versions.indexOf(sourceVersion)
        if (index < 0) index = 0
        targetVersion.currentIndex = index
        extended.checked = Boolean(info.extended)
        transparency.checked = Boolean(info.alpha)
        animations.checked = Boolean(info.durations)
        frameGroups.checked = Boolean(info.groups)
        serverExport.checked = Boolean(info.otb || info.itemsXml)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label { text: "Name" }
        TextField {
            id: nameField
            objectName: "compileAssetName"
            Layout.fillWidth: true
            selectByMouse: true
            placeholderText: "Tibia"
            validator: RegularExpressionValidator { regularExpression: /[A-Za-z0-9_-]{1,64}/ }
        }

        Label { text: "Output Folder" }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: outputFolder
                objectName: "compileAssetFolder"
                Layout.fillWidth: true
                selectByMouse: true
                placeholderText: "Choose a separate output folder"
            }
            Button { text: "Browse"; onClicked: folderPicker.open() }
        }

        GroupBox {
            title: "Server Items Export"
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                AssetToggle {
                    id: serverExport
                    text: "Export loaded items.otb and items.xml"
                    enabled: Boolean(backend.info.otb || backend.info.itemsXml)
                }
            }
        }

        GroupBox {
            title: "Settings"
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                Label { text: "Version" }
                ComboBox {
                    id: targetVersion
                    objectName: "compileAssetVersion"
                    Layout.fillWidth: true
                    model: compileDialog.versions
                    readonly property int selectedVersion: Number(currentText.replace(".", ""))
                    onActivated: compileDialog.setVersionDefaults()
                    popup: Popup {
                        y: targetVersion.height - 1
                        width: targetVersion.width
                        height: Math.min(240, contentItem.implicitHeight + topPadding + bottomPadding)
                        padding: 1
                        contentItem: ListView {
                            implicitHeight: contentHeight
                            clip: true
                            model: targetVersion.delegateModel
                            currentIndex: targetVersion.highlightedIndex
                            ScrollIndicator.vertical: ScrollIndicator {}
                        }
                        background: Rectangle {
                            color: Colors.surface
                            border.color: "#737d84"
                        }
                    }
                }
            }
        }

        GroupBox {
            title: "Options"
            Layout.fillWidth: true
            GridLayout {
                anchors.fill: parent
                columns: 2
                columnSpacing: 16
                AssetToggle { id: extended; objectName: "compileExtended"; text: "Extended" }
                AssetToggle { id: transparency; objectName: "compileTransparency"; text: "Transparency" }
                AssetToggle { id: animations; objectName: "compileAnimations"; text: "Improved animations" }
                AssetToggle { id: frameGroups; objectName: "compileFrameGroups"; text: "Frame Groups" }
            }
        }

        Label {
            text: "The open project stays unchanged. Turning off Frame Groups keeps only the first outfit group; turning off Improved animations removes frame timings."
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Colors.muted
        }
        Label {
            text: compileDialog.error
            visible: text.length > 0
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: "#ed9b9b"
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button {
                text: "Confirm"
                enabled: backend.loaded && nameField.acceptableInput && outputFolder.text.trim().length > 0
                onClicked: {
                    if (backend.compileAsOptions(outputFolder.text, nameField.text,
                                                 targetVersion.selectedVersion, extended.checked,
                                                 transparency.checked, animations.checked,
                                                 frameGroups.checked, serverExport.checked))
                        compileDialog.close()
                    else compileDialog.error = backend.status
                }
            }
            Button { text: "Cancel"; onClicked: compileDialog.close() }
        }
    }

    FolderDialog {
        id: folderPicker
        title: "Choose output folder"
        onAccepted: outputFolder.text = backend.localPath(selectedFolder.toString())
    }
}
