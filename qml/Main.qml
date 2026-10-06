import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: win
    width: 1672
    height: 941
    minimumWidth: 1100
    minimumHeight: 720
    visible: true
    title: "OTEditor" + (Backend.dirty ? " • Unsaved changes" : "")
    color: "#191d1f"
    font.family: "Segoe UI"
    font.pixelSize: 12
    palette {
        window: "#202426"
        windowText: "#e0e5e8"
        base: "#1c2022"
        placeholderText: "#8e9ba4"
        alternateBase: "#282d30"
        text: "#e0e5e8"
        button: "#2b3033"
        buttonText: "#e0e5e8"
        highlight: "#399ee8"
        highlightedText: "#ffffff"
        mid: "#3a4145"
        dark: "#151819"
        light: "#737d84"
    }
    property var d: Backend.details
    property int frame: 0
    property int pattern: 0
    property real previewZoom: 1
    readonly property int spriteSize: Number(String(Backend.info.spriteDimension || "32x32").split("x")[0]) || 32
    property int selectedSprite: 0
    property bool showLog: true
    property bool showPreviewPanel: true
    property bool showObjectsPanel: true
    property bool showSpritesPanel: true
    property int browserMode: 1
    property string objectSearchQuery: ""
    property int spriteMode: 0
    property bool closingApproved: false
    property bool pendingOpen: false
    property bool pendingNew: false
    property string spriteImportMode: "replace"
    property string allExportMode: "objects"
    readonly property string currentImage: {
        let revision = Backend.revision;
        return Backend.preview(Backend.selected, frame,
                               Backend.category === 1 ? attributes.outfitPattern : pattern,
                               Backend.category === 1 ? attributes.outfitGroup : 0,
                               Backend.category === 1 ? attributes.outfitLayer : -1);
    }
    readonly property bool editable: Backend.loaded && Backend.category === 0 && Backend.selected >= 0
    readonly property bool objectEditable: Backend.loaded && Backend.selected >= 0
    function openProject() {
        if (Backend.dirty) {
            pendingOpen = true;
            unsaved.open();
        } else
            loadDialog.open();
    }
    function newProject() {
        if (Backend.dirty) {
            pendingNew = true;
            unsaved.open();
        } else
            newAssetDialog.open();
    }
    function openObjectExport() {
        objectExportDialog.open();
    }
    function toggleObjectsGridView() {
        browserMode = browserMode === 1 ? 0 : 1;
    }
    function refreshFilter() {
        Backend.filter(browserMode === 2 ? objectSearchQuery : "", false);
    }
    onBrowserModeChanged: refreshFilter()
    onClosing: function (close) {
        if (Backend.dirty && !closingApproved) {
            close.accepted = false;
            pendingOpen = false;
            pendingNew = false;
            unsaved.open();
        }
    }
    Connections {
        target: Backend
        function onChanged() {
            objectsPanel.revealSelection();
            if (win.frame >= (win.d.frames || 1))
                win.frame = 0;
            if (win.pattern >= ((win.d.patternX || 1) * (win.d.patternY || 1) * (win.d.patternZ || 1)))
                win.pattern = 0;
        }
    }
    Shortcut {
        sequences: [StandardKey.Undo]
        enabled: Backend.canUndo
        onActivated: Backend.undo()
    }
    Shortcut {
        sequences: [StandardKey.Redo]
        enabled: Backend.canRedo
        onActivated: Backend.redo()
    }
    Shortcut {
        sequence: "Ctrl+D"
        enabled: win.editable
        onActivated: Backend.create(true)
    }
    component CompactDropdown: Menu {
        width: 205
        delegate: CompactContextItem {}
    }
    menuBar: MenuBar {
        height: 34
        leftPadding: 8
        background: Rectangle {
            color: "#1c2022"
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#303638" }
        }
        delegate: MenuBarItem {
            id: menuItem
            implicitWidth: contentItem.implicitWidth + 32
            implicitHeight: 34
            contentItem: Text {
                text: menuItem.text
                font.family: "Segoe UI"
                font.pixelSize: 14
                color: "#edf0f2"
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
            }
            background: Rectangle {
                radius: 4
                color: menuItem.highlighted ? "#2b3134" : "transparent"
            }
        }
        CompactDropdown {
            title: "File"
            Action {
                text: "New"
                shortcut: "Ctrl+N"
                onTriggered: win.newProject()
            }
            Action {
                text: "Open"
                shortcut: StandardKey.Open
                onTriggered: win.openProject()
            }
            Action {
                text: "New Window"
                onTriggered: Backend.openNewWindow()
            }
            Action {
                text: "Compile"
                shortcut: StandardKey.Save
                enabled: Backend.loaded
                onTriggered: Backend.compile()
            }
            Action {
                text: "Compile As..."
                shortcut: StandardKey.SaveAs
                enabled: Backend.loaded
                onTriggered: compileAssetDialog.open()
            }
            Action {
                text: "Export All Objects..."
                enabled: Backend.loaded
                onTriggered: { win.allExportMode="objects"; allExportFolderDialog.open() }
            }
            Action {
                text: "Export All Animation Sheets..."
                enabled: Backend.loaded
                onTriggered: { win.allExportMode="sheets"; allExportFolderDialog.open() }
            }
            Action {
                text: "Export All Sprites..."
                enabled: Backend.loaded
                onTriggered: { win.allExportMode="sprites"; allExportFolderDialog.open() }
            }
            MenuSeparator {
                implicitHeight: 7
            }
            Action {
                text: "Close"
                shortcut: "Ctrl+W"
                onTriggered: win.close()
            }
            MenuSeparator {
                implicitHeight: 7
            }
            Action {
                text: "Merge..."
                shortcut: "Ctrl+M"
                enabled: Backend.loaded
                onTriggered: mergeDialog.open()
            }
            Action {
                text: "Convert Project..."
                enabled: Backend.loaded
                onTriggered: convertProjectDialog.open()
            }
            MenuSeparator {
                implicitHeight: 7
            }
            Action {
                text: "Preferences"
                shortcut: "Ctrl+P"
                onTriggered: preferencesDialog.open()
            }
            MenuSeparator {
                implicitHeight: 7
            }
            Action {
                text: "Exit"
                shortcut: "Ctrl+Q"
                onTriggered: win.close()
            }
        }
        CompactDropdown {
            title: "View"
            Action {
                objectName: "showPreviewPanelAction"
                text: "Show Preview Panel"
                shortcut: "Shift+F2"
                checkable: true
                checked: win.showPreviewPanel
                onTriggered: win.showPreviewPanel = !win.showPreviewPanel
            }
            Action {
                objectName: "showObjectsPanelAction"
                text: "Show Objects Panel"
                shortcut: "Shift+F3"
                checkable: true
                checked: win.showObjectsPanel
                onTriggered: win.showObjectsPanel = !win.showObjectsPanel
            }
            Action {
                objectName: "showSpritesPanelAction"
                text: "Show Sprites Panel"
                shortcut: "Shift+F4"
                checkable: true
                checked: win.showSpritesPanel
                onTriggered: win.showSpritesPanel = !win.showSpritesPanel
            }
            Action {
                objectName: "showObjectsGridAction"
                text: "Show Objects Grid"
                shortcut: "Shift+F5"
                onTriggered: win.toggleObjectsGridView()
            }
        }
        CompactDropdown {
            title: "Tools"
            width: 230
            Action {
                objectName: "findToolAction"
                text: "Find"
                shortcut: "Ctrl+F"
                enabled: Backend.loaded
                onTriggered: {
                    win.showObjectsPanel = true;
                    win.browserMode = 2;
                    findDialog.open();
                }
            }
            Action {
                text: "LookType Generator"
                enabled: Backend.loaded
                onTriggered: lookTypeDialog.open()
            }
            Action {
                text: "AI Sprite Generator"
                onTriggered: aiSpriteDialog.open()
            }
            Action {
                text: "Object Viewer"
                enabled: Backend.loaded
                onTriggered: objectViewer.open()
            }
            Action {
                text: "Slicer"
                onTriggered: slicerDialog.open()
            }
            Action {
                text: "Animation Editor"
                enabled: Backend.loaded && Backend.selected >= 0 && Backend.info.durations
                onTriggered: animationDialog.open()
            }
            Action {
                text: "Sprites Optimizer"
                enabled: Backend.loaded
                onTriggered: spriteOptimizer.open()
            }
            Action {
                text: "Frame Durations Optimizer"
                enabled: Backend.loaded && Backend.info.durations
                onTriggered: durationOptimizer.open()
            }
            Action {
                text: "Frame Durations Converter"
                enabled: Backend.loaded
                onTriggered: durationConverter.open()
            }
            Action {
                text: "Frame Groups Converter"
                enabled: Backend.loaded
                onTriggered: frameGroupsConverter.open()
            }
            Action {
                text: "Bulk Replace Objects"
                enabled: Backend.loaded && Backend.category === 0 && Backend.selectedCount > 0
                onTriggered: bulkReplaceDialog.open()
            }
            MenuSeparator {
                implicitHeight: 7
            }
            Action {
                text: "Create items.otb"
                enabled: Backend.loaded && !Boolean(Backend.info.otb)
                onTriggered: Backend.createOtbFile()
            }
            Action {
                objectName: "createMissingOtbItemsAction"
                text: "Create Missing OTB Items"
                enabled: Boolean(Backend.info.otb)
                onTriggered: Backend.createMissingOtbItems()
            }
            Action {
                objectName: "reloadItemAttributesAction"
                text: "Reload Item Attributes"
                enabled: Boolean(Backend.info.otb)
                onTriggered: Backend.reloadItemAttributes()
            }
            Action {
                text: "Reload Selected OTB Item"
                enabled: Backend.serverId >= 0
                onTriggered: Backend.reloadSelectedOtbItem()
            }
            Action {
                text: "Update OTB Version..."
                enabled: Boolean(Backend.info.otb)
                onTriggered: otbVersionDialog.open()
            }
            Action {
                text: "Compare items.otb..."
                enabled: Boolean(Backend.info.otb)
                onTriggered: compareOtbDialog.open()
            }
            Action {
                text: "Edit items.xml Attributes..."
                enabled: Boolean(Backend.info.itemsXml) && Backend.serverId > 0
                onTriggered: xmlAttributesDialog.open()
            }
        }
        CompactDropdown {
            title: "Help"
            Action {
                text: "Check for Updates..."
                onTriggered: {
                    updateDialog.open()
                    Updater.checkForUpdates()
                }
            }
            Action {
                text: "About OTEditor"
                onTriggered: about.open()
            }
        }
    }
    Dialog {
        id: updateDialog
        title: "OTEditor Update"
        width: 440
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.NoButton
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: Updater.state === "checking" ? "Checking for updates..."
                    : Updater.state === "downloading" ? "Downloading version " + Updater.latestVersion + "..."
                    : Updater.state === "installing" ? "Preparing installation..."
                    : Updater.state === "available" ? "Version " + Updater.latestVersion + " is available."
                    : Updater.state === "error" ? Updater.errorString
                    : "OTEditor is up to date (version " + Updater.currentVersion + ")."
            }
            ProgressBar {
                Layout.fillWidth: true
                visible: Updater.state === "downloading"
                value: Updater.downloadProgress
            }
            Label {
                Layout.fillWidth: true
                visible: Updater.state === "available" && Updater.releaseNotes.length > 0
                text: Updater.releaseNotes
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button {
                    text: "Open Release"
                    visible: Updater.releasePageUrl.toString().length > 0
                    onClicked: Updater.openReleasePage()
                }
                Button {
                    text: "Install Update"
                    visible: Updater.updateAvailable
                    enabled: !Updater.busy && !Backend.dirty
                    onClicked: Updater.downloadAndInstall()
                }
                Button {
                    text: Updater.busy ? "Cancel" : "Close"
                    onClicked: {
                        if (Updater.busy) Updater.cancel()
                        updateDialog.close()
                    }
                }
            }
        }
    }
    header: FluentToolBar {
        backend: Backend
        owner: win
        playing: attributes.animate
        onNewRequested: win.newProject()
        onOpenRequested: win.openProject()
        onRemoveRequested: removeItemConfirm.open()
        onPlaybackRequested: attributes.animate = !attributes.animate
        onPropertiesRequested: attributes.showProperties()
    }
    ColumnLayout {
        anchors {
            fill: parent
            margins: 9
        }
        spacing: 8
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal
            handle: Rectangle {
                implicitWidth: 8
                color: "transparent"
                Rectangle {
                    anchors.centerIn: parent
                    width: 2
                    height: 28
                    radius: 1
                    color: SplitHandle.hovered ? "#318ac0" : "#424a54"
                }
            }
            PreviewSidebar {
                backend: Backend
                owner: win
            }
            ObjectsBrowserPanel {
                id: objectsPanel
                backend: Backend
                owner: win
                onFindRequested: findDialog.open()
                onContextMenuRequested: (x, y) => objectMenu.popup(x, y)
                onAttributesRequested: attributes.showProperties()
                onAssignRequested: assign.open()
                onRemoveRequested: removeItemConfirm.open()
            }
            Panel {
                title: "Object Editor" + (win.d.itemId !== undefined ? " — ID: " + win.d.itemId : "")
                SplitView.fillWidth: true
                SplitView.minimumWidth: 440
                ItemInspector {
                    id: attributes
                    anchors.fill: parent
                    onPreviewFrameChanged: win.frame = previewFrame
                }
            }
            SpritesBrowserPanel {
                backend: Backend
                owner: win
                onContextMenuRequested: (x, y) => spriteMenu.popup(x, y)
                onAssignRequested: assign.open()
                onImportRequested: mode => {
                    win.spriteImportMode = mode;
                    spriteImportDialog.open();
                }
                onRemoveRequested: removeSpriteConfirm.open()
            }
        }
        EditorLogPanel {
            backend: Backend
            owner: win
        }
    }
    footer: Rectangle {
        height: 26
        color: "#252b32"
        border.color: "#39424c"
        RowLayout {
            anchors {
                fill: parent
                leftMargin: 10
                rightMargin: 10
            }
            Label {
                text: Backend.status
                color: "#b4c7da"
                font.pixelSize: 11
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Tool {
                text: "Export…"
                implicitHeight: 22
                enabled: Backend.selected >= 0
                onClicked: win.openObjectExport()
            }
        }
    }
    component CompactContextItem: MenuItem {
        id: compactItem
        implicitHeight: 26
        leftPadding: 9
        rightPadding: 9
        font.pixelSize: 11
        contentItem: Text {
            leftPadding: compactItem.checkable ? 32 : 0
            text: compactItem.text
            font: compactItem.font
            color: compactItem.enabled ? "#e0e5e8" : "#77828e"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
    Menu {
        id: objectMenu
        objectName: "objectContextMenu"
        parent: win.contentItem
        width: 190
        CompactContextItem {
            text: "Replace"
            enabled: win.editable
            onTriggered: replaceDialog.open()
        }
        CompactContextItem {
            objectName: "importClientObjectsContextAction"
            text: "Import Objects from Client..."
            enabled: win.editable
            onTriggered: importGraphicsDialog.openForTarget(Number(win.d.itemId || 100))
        }
        CompactContextItem {
            text: "Import OBD..."
            enabled: win.objectEditable && Backend.info.spriteDimension === "32x32"
            onTriggered: obdImportDialog.open()
        }
        CompactContextItem {
            text: "Export"
            enabled: Backend.selected >= 0
            onTriggered: win.openObjectExport()
        }
        CompactContextItem {
            text: "Edit"
            enabled: win.editable
            onTriggered: attributes.showProperties()
        }
        CompactContextItem {
            text: "Duplicate"
            enabled: win.objectEditable
            onTriggered: Backend.create(true)
        }
        CompactContextItem {
            text: "Bulk Edit"
            enabled: Backend.category === 0 && Backend.selectedCount > 0
            onTriggered: bulkEditDialog.open()
        }
        CompactContextItem {
            text: "Copy Object"
            enabled: win.editable
            onTriggered: Backend.copyObjectPart("object")
        }
        CompactContextItem {
            text: "Paste Object"
            enabled: win.editable && Backend.objectClipboard.object
            onTriggered: Backend.pasteObjectPart("object")
        }
        CompactContextItem {
            text: "Copy Patterns"
            enabled: win.editable
            onTriggered: Backend.copyObjectPart("patterns")
        }
        CompactContextItem {
            text: "Paste Patterns"
            enabled: win.editable && Backend.objectClipboard.patterns
            onTriggered: Backend.pasteObjectPart("patterns")
        }
        CompactContextItem {
            text: "Copy Properties"
            enabled: win.editable
            onTriggered: Backend.copyObjectPart("properties")
        }
        CompactContextItem {
            text: "Paste Properties"
            enabled: win.editable && Backend.objectClipboard.properties
            onTriggered: Backend.pasteObjectPart("properties")
        }
        CompactContextItem {
            text: "Copy Attributes"
            enabled: Backend.serverId >= 0
            onTriggered: Backend.copyServerAttributes()
        }
        CompactContextItem {
            text: "Paste Attributes"
            enabled: Backend.serverId >= 0 && Backend.hasCopiedServerAttributes
            onTriggered: Backend.pasteServerAttributes()
        }
        CompactContextItem {
            text: "Remove object"
            enabled: win.objectEditable
            onTriggered: removeItemConfirm.open()
        }
        MenuSeparator {
            implicitHeight: 7
        }
        CompactContextItem {
            text: "Compare…"
            enabled: Backend.selectedCount === 2
            onTriggered: {
                const comparison = Backend.compareSelectedObjects();
                objectComparisonText.text = comparison.error ||
                    (comparison.firstId + " vs " + comparison.secondId + "\n\n" +
                     (comparison.differences.length ? comparison.differences.join("\n") : "No differences"));
                objectComparisonDialog.open();
            }
        }
        MenuSeparator {
            implicitHeight: 7
        }
        CompactContextItem {
            text: "Copy Client ID: " + (win.d.itemId ?? "-")
            enabled: Backend.selected >= 0
            onTriggered: Backend.copyId()
        }
        CompactContextItem {
            text: "Copy Server ID: " + (Backend.serverId >= 0 ? Backend.serverId : "-")
            enabled: Backend.serverId >= 0
            onTriggered: Backend.copyId(true)
        }
    }
    Menu {
        id: spriteMenu
        parent: win.contentItem
        width: 180
        CompactContextItem {
            text: "Export PNG…"
            enabled: Backend.loaded && win.selectedSprite > 0
            onTriggered: spriteExportDialog.open()
        }
    }
    FindObjectsDialog {
        id: findDialog
        backend: Backend
        owner: win
    }
    ImportClientObjectsDialog {
        id: importGraphicsDialog
        backend: Backend
        owner: win
    }
    ReplaceItemDialog {
        id: replaceDialog
        backend: Backend
        owner: win
    }
    CreateAssetFilesDialog {
        id: newAssetDialog
        backend: Backend
        owner: win
    }
    CompileAssetFilesDialog { id: compileAssetDialog; backend: Backend }
    FileDialog {
        id: spriteExportDialog
        title: "Export sprite PNG"
        fileMode: FileDialog.SaveFile
        nameFilters: ["PNG image (*.png)"]
        defaultSuffix: "png"
        onAccepted: Backend.exportSprite(selectedFile.toString(), win.selectedSprite)
    }
    ExportObjectsDialog {
        id: objectExportDialog
        backend: Backend
        owner: win
    }
    CompileProgressPopup {
        id: compilePopup
        backend: Backend
        owner: win
    }
    FileDialog {
        id: spriteImportDialog
        title: win.spriteImportMode === "add" ? "Add " + win.spriteSize + "×" + win.spriteSize + " sprite" : "Replace sprite " + win.selectedSprite
        fileMode: FileDialog.OpenFile
        nameFilters: ["PNG image (*.png)"]
        onAccepted: {
            if (win.spriteImportMode === "add") {
                let id = Backend.addSprite(selectedFile.toString());
                if (id > 0)
                    win.selectedSprite = id;
            } else
                Backend.replaceSprite(win.selectedSprite, selectedFile.toString());
        }
    }
    OpenAssetFilesDialog {
        id: loadDialog
        backend: Backend
        owner: win
    }
    Dialog {
        id: unsaved
        title: "Unsaved changes"
        anchors.centerIn: parent
        width: 430
        modal: true
        standardButtons: Dialog.Save | Dialog.Discard | Dialog.Cancel
        Label {
            text: "Save your DAT changes before continuing?"
        }
        function proceed() {
            if (win.pendingNew) {
                win.pendingNew = false;
                newAssetDialog.open();
            } else if (win.pendingOpen) {
                win.pendingOpen = false;
                loadDialog.open();
            } else {
                win.closingApproved = true;
                win.close();
            }
        }
        onAccepted: if (Backend.compile())
            proceed()
        onDiscarded: proceed()
    }
    Dialog {
        id: removeSpriteConfirm
        title: "Clear sprite " + win.selectedSprite
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            text: "The sprite slot will become transparent. Later IDs stay unchanged.\nObjects referencing this ID will display an empty sprite."
        }
        onAccepted: Backend.removeSprite(win.selectedSprite)
    }
    Dialog {
        id: removeItemConfirm
        objectName: "removeItemConfirm"
        title: "Remove object " + (win.d.itemId ?? "")
        anchors.centerIn: parent
        width: 490
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            width: 440
            wrapMode: Text.WordWrap
            text: Backend.category === 0
                  ? "Remove the selected item? Its server item mapping will also be removed. Other client IDs stay unchanged."
                  : "Remove the selected object? Other IDs in this category stay unchanged."
        }
        onAccepted: Backend.removeObject()
    }
    Dialog {
        id: assign
        title: "Assign sprite"
        anchors.centerIn: parent
        width: 460
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Label {
                text: "Item " + (win.d.itemId ?? "") + " · sprite " + win.selectedSprite
                font.pixelSize: 17
            }
            Label {
                text: "Choose a slot in the object's sprite array.\nSlots include tiles, layers, patterns and animation frames."
                color: "#9aafc1"
            }
            RowLayout {
                Label {
                    text: "Slot"
                }
                SpinBox {
                    id: slot
                    from: 0
                    to: Math.max(0, (win.d.spriteIds || []).length - 1)
                    editable: true
                }
            }
            Label {
                text: "Current sprite: " + ((win.d.spriteIds || [])[slot.value] ?? "—")
                color: "#91c7ec"
            }
        }
        onAccepted: Backend.assignSprite(slot.value, win.selectedSprite)
    }
    LookTypeDialog {
        id: lookTypeDialog
        backend: Backend
        owner: win
    }
    ObjectViewerDialog {
        id: objectViewer
        backend: Backend
        owner: win
    }
    SlicerWindow {
        id: slicerDialog
        objectName: "slicerDialog"
        backend: Backend
        clientSpriteSize: win.spriteSize
    }
    AnimationEditorDialog {
        id: animationDialog
        backend: Backend
        owner: win
    }
    FrameDurationsOptimizerDialog {
        id: durationOptimizer
        backend: Backend
        owner: win
    }
    PreferencesDialog { id: preferencesDialog; backend: Backend }
    MergeProjectDialog { id: mergeDialog; backend: Backend }
    ConvertProjectDialog { id: convertProjectDialog; backend: Backend }
    XmlAttributesDialog { id: xmlAttributesDialog; backend: Backend }
    FolderDialog {
        id: allExportFolderDialog
        title: win.allExportMode === "sprites" ? "Export all sprites" : "Export all objects"
        onAccepted: {
            if (win.allExportMode === "sprites") Backend.exportAllSprites(selectedFolder.toString());
            else Backend.exportAllObjects(selectedFolder.toString(),win.allExportMode === "sheets");
        }
    }
    FileDialog {
        id: compareOtbDialog
        title: "Compare with items.otb"
        fileMode: FileDialog.OpenFile
        nameFilters: ["OTB files (*.otb)"]
        onAccepted: {
            const comparison = Backend.compareOtbFile(selectedFile.toString());
            comparisonText.text = comparison.error ||
                ("Changed: " + comparison.changed + " · Only in current: " + comparison.removed +
                 " · Only in comparison: " + comparison.added + "\n\n" + comparison.differences.join("\n"));
            compareOtbResult.open();
        }
    }
    FileDialog {
        id: obdImportDialog
        title: "Import Object Builder OBD"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Object Builder Data (*.obd)"]
        onAccepted: Backend.importObd(selectedFile.toString())
    }
    Dialog {
        id: compareOtbResult
        title: "OTB comparison"
        width: 540
        height: 440
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Close
        ScrollView { anchors.fill: parent; TextArea { id: comparisonText; readOnly: true; wrapMode: TextEdit.Wrap } }
    }
    Dialog {
        id: objectComparisonDialog
        title: "Compare selected objects"
        width: 520
        height: 400
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Close
        ScrollView { anchors.fill: parent; TextArea { id: objectComparisonText; readOnly: true; wrapMode: TextEdit.Wrap } }
    }
    Dialog {
        id: bulkReplaceDialog
        title: "Replace selected items"
        width: 390
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: Backend.bulkReplaceObjects(bulkSourceId.value)
        ColumnLayout {
            Label { text: "Copy the complete object from this client ID into " + Backend.selectedCount + " selected item(s). Target IDs are preserved."; wrapMode: Text.Wrap; Layout.preferredWidth: 340 }
            RowLayout {
                Label { text: "Source client ID" }
                SpinBox { id: bulkSourceId; from: 100; to: 65535; value: 100; editable: true }
            }
        }
    }
    Dialog {
        id: bulkEditDialog
        title: "Bulk edit selected items"
        width: 390
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: Backend.bulkSetItemAttribute(bulkAttribute.currentValue,bulkFlag.checked)
        ColumnLayout {
            Label { text: "Apply one property to " + Backend.selectedCount + " selected item(s)." }
            ComboBox {
                id: bulkAttribute
                Layout.fillWidth: true
                textRole: "text"
                valueRole: "key"
                model: [
                    {text:"Stackable",key:"isStackable"},{text:"Container",key:"isContainer"},
                    {text:"Unpassable",key:"isUnpassable"},{text:"Unmoveable",key:"isUnmoveable"},
                    {text:"Blocks missiles",key:"blocksMissiles"},{text:"Blocks pathfinder",key:"blocksPathfinder"},
                    {text:"Pickupable",key:"isPickupable"},{text:"Useable",key:"isUseable"},
                    {text:"Rotatable",key:"isRotatable"},{text:"Hangable",key:"isHangable"},
                    {text:"Animate always",key:"animateAlways"},{text:"Ignore look",key:"ignoreLook"}
                ]
            }
            CheckBox { id: bulkFlag; text: "Enabled"; checked: true }
        }
    }
    Dialog {
        id: otbVersionDialog
        title: "Update OTB version"
        width: 370
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            otbMajor.value = Number(Backend.info.otbMajor);
            otbMinor.value = Number(Backend.info.otbMinor);
            otbBuild.value = Number(Backend.info.otbBuild);
        }
        onAccepted: Backend.updateOtbVersion(otbMajor.value, otbMinor.value, otbBuild.value)
        ColumnLayout {
            RowLayout {
                Label { text: "Major"; Layout.preferredWidth: 65 }
                SpinBox { id: otbMajor; from: 0; to: 999999; editable: true }
            }
            RowLayout {
                Label { text: "Minor"; Layout.preferredWidth: 65 }
                SpinBox { id: otbMinor; from: 0; to: 999999; editable: true }
            }
            RowLayout {
                Label { text: "Build"; Layout.preferredWidth: 65 }
                SpinBox { id: otbBuild; from: 0; to: 999999; editable: true }
            }
        }
    }
    FrameDurationsConverterDialog {
        id: durationConverter
        backend: Backend
    }
    FrameGroupsConverterDialog {
        id: frameGroupsConverter
        backend: Backend
        owner: win
    }
    SpritesOptimizerDialog {
        id: spriteOptimizer
        backend: Backend
        owner: win
    }
    AboutDialog {
        id: about
    }
    AiSpriteGeneratorDialog {
        id: aiSpriteDialog
        service: AiSprites
        onSlicerRequested: imageUrl => {
            if (slicerDialog.openImage(imageUrl.toString())) slicerDialog.open()
        }
    }
}
