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
    color: "#1e2329"
    font.family: "Segoe UI"
    font.pixelSize: 12
    palette {
        window: "#24292f"
        windowText: "#dce5ee"
        base: "#20252b"
        placeholderText: "#718294"
        alternateBase: "#2c333b"
        text: "#dce5ee"
        button: "#333b44"
        buttonText: "#dce5ee"
        highlight: "#17679d"
        highlightedText: "#ffffff"
        mid: "#424d59"
        dark: "#161b20"
        light: "#536171"
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
        background: Rectangle {
            color: "#252b32"
            border.color: "#3b424b"
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
                onTriggered: compileFolderDialog.open()
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
                text: "Save items.otb"
                enabled: Boolean(Backend.info.otb)
                onTriggered: Backend.saveOtbFile()
            }
            Action {
                text: "Save items.otb Copy..."
                enabled: Boolean(Backend.info.otb)
                onTriggered: saveOtbCopyDialog.open()
            }
            Action {
                text: "Save items.xml"
                enabled: Boolean(Backend.info.itemsXml)
                onTriggered: Backend.saveItemsXmlFile()
            }
            Action {
                text: "Save items.xml Copy..."
                enabled: Boolean(Backend.info.itemsXml)
                onTriggered: saveItemsXmlCopyDialog.open()
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
        }
        CompactDropdown {
            title: "Help"
            Action {
                text: "About OTEditor"
                onTriggered: about.open()
            }
        }
    }
    header: ToolBar {
        height: 44
        background: Rectangle {
            color: "#20262d"
            border.color: "#39434e"
        }
        RowLayout {
            anchors {
                fill: parent
                leftMargin: 10
                rightMargin: 12
            }
            spacing: 7
            Tool {
                text: "▱"
                font.pixelSize: 21
                tip: "Open client folder · Ctrl+O"
                onClicked: win.openProject()
            }
            Tool {
                text: "▣"
                font.pixelSize: 18
                tip: "Compile DAT + SPR · Ctrl+S"
                enabled: Backend.loaded
                onClicked: Backend.compile()
            }
            Rectangle {
                implicitWidth: 1
                implicitHeight: 23
                color: "#454b53"
                Layout.leftMargin: 3
                Layout.rightMargin: 3
            }
            Tool {
                text: "+"
                font.pixelSize: 20
                tip: "New object"
                enabled: Backend.loaded
                onClicked: Backend.create()
            }
            Tool {
                text: "⧉"
                font.pixelSize: 19
                tip: "Duplicate object · Ctrl+D"
                enabled: win.objectEditable
                onClicked: Backend.create(true)
            }
            Tool {
                text: "×"
                font.pixelSize: 18
                tip: "Remove selected object"
                enabled: win.objectEditable
                onClicked: removeItemConfirm.open()
            }
            Tool {
                text: "↶"
                font.pixelSize: 21
                tip: "Undo · Ctrl+Z"
                enabled: Backend.canUndo
                onClicked: Backend.undo()
            }
            Tool {
                text: "↷"
                font.pixelSize: 21
                tip: "Redo · Ctrl+Y"
                enabled: Backend.canRedo
                onClicked: Backend.redo()
            }
            Rectangle {
                implicitWidth: 1
                implicitHeight: 23
                color: "#454b53"
            }
            Tool {
                text: attributes.animate ? "Ⅱ" : "▶"
                tip: "Play animation"
                checked: attributes.animate
                enabled: Backend.loaded
                onClicked: attributes.animate = !attributes.animate
            }
            Tool {
                text: "⚙"
                font.pixelSize: 18
                tip: "Item attributes"
                enabled: win.editable
                onClicked: attributes.showProperties()
            }
            Item {
                Layout.fillWidth: true
            }
            Label {
                text: Backend.loaded ? Backend.info.folder : "DAT / SPR  •  QML + C++"
                color: "#8e9ba9"
                elide: Text.ElideMiddle
                Layout.maximumWidth: 650
            }
            Rectangle {
                width: 6
                height: 6
                radius: 3
                color: Backend.dirty ? "#e7b467" : Backend.loaded ? "#75bb99" : "#7b8793"
            }
            Label {
                text: Backend.dirty ? "Modified" : Backend.loaded ? "Ready" : "No client loaded"
                color: "#a6b4c3"
            }
        }
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
            color: compactItem.enabled ? "#dce5ee" : "#77828e"
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
    FolderDialog {
        id: compileFolderDialog
        title: "Compile complete project to folder"
        onAccepted: Backend.compileAs(selectedFolder.toString())
    }
    FileDialog {
        id: saveOtbCopyDialog
        title: "Save items.otb copy"
        fileMode: FileDialog.SaveFile
        nameFilters: ["OTB files (*.otb)"]
        defaultSuffix: "otb"
        onAccepted: Backend.saveOtbFile(selectedFile.toString())
    }
    FileDialog {
        id: saveItemsXmlCopyDialog
        title: "Save items.xml copy"
        fileMode: FileDialog.SaveFile
        nameFilters: ["XML files (*.xml)"]
        defaultSuffix: "xml"
        onAccepted: Backend.saveItemsXmlFile(selectedFile.toString())
    }
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
        title: "Remove object " + (win.d.itemId ?? "")
        anchors.centerIn: parent
        width: 490
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            width: 440
            wrapMode: Text.WordWrap
            text: Backend.category === 0
                  ? "Remove the selected item? Later client IDs will shift down by one.\nServer item mappings may need to be updated."
                  : "Remove the selected object? Later IDs in this category will shift down by one."
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
}
