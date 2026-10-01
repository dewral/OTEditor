import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: inspector
    objectName: "objectInspector"
    property string selectedKey: ""
    property string projectKey: ""
    property var pendingDrafts: ({})
    readonly property bool isItem: Backend.category===0
    readonly property bool isOutfit: Backend.category===1
    readonly property bool editable: Backend.loaded && inspector.isItem && Backend.selected>=0
    readonly property int spriteSize: Number(String(Backend.info.spriteDimension || "32x32").split("x")[0]) || 32
    function showProperties() { tabIndex=1; forceActiveFocus() }
    function syncSelection() {
        if (tabIndex === 2 && (!isItem || !Boolean(Backend.info.otb))) tabIndex = 0
        let folder=String(Backend.info.folder||"")
        if(folder!==projectKey) { projectKey=folder; selectedKey=""; pendingDrafts=({}); changes=({}) }
        let key=Backend.category+":"+(Backend.details.itemId ?? "")
        if(key!==selectedKey) {
            if(selectedKey.length && Object.keys(changes).length) pendingDrafts[selectedKey]=changes
            selectedKey=key
            changes=pendingDrafts[key] || ({})
            previewFrame=0; outfitGroup=0; outfitLayer=0; outfitDirection=2; animate=false
            Qt.callLater(function() { if (textureScroll.contentItem) textureScroll.contentItem.contentY=0 })
        }
        draft=Object.assign({},Backend.details,changes)
    }
    Connections { target: Backend; function onChanged() { inspector.syncSelection(); inspector.syncDurationFields(); if(Object.keys(inspector.serverChanges).length===0) inspector.resetServer() } }
    Component.onCompleted: { syncSelection(); syncDurationFields(); resetServer() }
    property var draft: ({})
    property var changes: ({})
    property var serverDraft: ({})
    property var serverChanges: ({})
    property int tabIndex: 0
    property real textureZoom: 1
    property bool editPixelsMode: false
    property int previewFrame: 0
    property int outfitGroup: 0
    property int outfitLayer: 0
    property int outfitDirection: 2
    readonly property var activeFrameGroup: inspector.isOutfit && inspector.draft.frameGroups && inspector.draft.frameGroups.length
        ? inspector.draft.frameGroups[Math.min(inspector.outfitGroup,inspector.draft.frameGroups.length-1)] : inspector.draft
    readonly property var savedFrameGroup: inspector.isOutfit && Backend.details.frameGroups && Backend.details.frameGroups.length
        ? Backend.details.frameGroups[Math.min(inspector.outfitGroup,Backend.details.frameGroups.length-1)] : Backend.details
    readonly property int patternColumns: Math.max(1,Number(inspector.activeFrameGroup.patternX||1))
    readonly property int patternCount: inspector.patternColumns*Math.max(1,Number(inspector.activeFrameGroup.patternY||1))*Math.max(1,Number(inspector.activeFrameGroup.patternZ||1))
    readonly property int previewColumns: inspector.isOutfit ? 1 : inspector.patternColumns
    readonly property int shownPatternCount: inspector.isOutfit ? 1 : Math.min(256,inspector.patternCount)
    readonly property int savedPatternCount: Math.max(1,Number(inspector.savedFrameGroup.patternX||1))*Math.max(1,Number(inspector.savedFrameGroup.patternY||1))*Math.max(1,Number(inspector.savedFrameGroup.patternZ||1))
    readonly property real patternCellWidth: Math.max(1,Number(inspector.activeFrameGroup.itemWidth)||1)*inspector.spriteSize*inspector.textureZoom
    readonly property real patternCellHeight: Math.max(1,Number(inspector.activeFrameGroup.itemHeight)||1)*inspector.spriteSize*inspector.textureZoom
    readonly property int outfitPattern: inspector.isOutfit ? Math.min(inspector.outfitDirection,Math.max(0,Number(inspector.activeFrameGroup.patternX||1)-1)) : 0
    property bool cropVisible: false
    property bool gridVisible: true
    property bool filmVisible: false
    property bool animate: false
    property string saveError: ""
    onPreviewFrameChanged: syncDurationFields()
    onOutfitGroupChanged: syncDurationFields()
    function syncDurationFields() {
        let data=Backend.frameDuration(Backend.category,Number(Backend.details.itemId||0),inspector.isOutfit ? outfitGroup : 0,previewFrame)
        minimumDuration.value=Number(data.minimum||100)
        maximumDuration.value=Number(data.maximum||100)
    }
    function applyFrameDuration(changedField) {
        if (!Backend.loaded || !Boolean(Backend.info.durations)) return
        if (minimumDuration.value>maximumDuration.value) {
            if (changedField==="minimum") maximumDuration.value=minimumDuration.value
            else minimumDuration.value=maximumDuration.value
        }
        if (!Backend.setFrameDuration(Backend.category,Number(Backend.details.itemId||0),inspector.isOutfit ? outfitGroup : 0,previewFrame,minimumDuration.value,maximumDuration.value))
            syncDurationFields()
    }
    function totalDuration(field) {
        let revision=Backend.revision
        let frames=Math.max(1,Number(inspector.activeFrameGroup.frames||1))
        let total=0
        for (let frame=0;frame<frames;++frame) {
            let duration=Backend.frameDuration(Backend.category,Number(Backend.details.itemId||0),inspector.isOutfit ? outfitGroup : 0,frame)
            total+=Number(duration[field]||100)
        }
        return total
    }
    readonly property int clientVersion: Math.round(Number(Backend.info.version || 0)*100)
    function edit(key, value) {
        let next=Object.assign({},draft); next[key]=value; draft=next
        let pending=Object.assign({},changes); pending[key]=value; changes=pending
        saveError=""
    }
    function resetDraft() {
        if (Backend.textureEditPending) Backend.resetTextureEdit()
        draft=Object.assign({},Backend.details); changes=({}); delete pendingDrafts[selectedKey]
        saveError=""; previewFrame=Math.min(previewFrame,(draft.frames||1)-1)
    }
    function saveDraft() {
        if (Object.keys(changes).length && !Backend.setValues(changes)) {
            saveError="Could not save these values. Check the selected DAT version and field limits."
            return
        }
        if (Backend.textureEditPending && !Backend.saveTextureEdit()) {
            saveError="Could not save the texture changes."
            return
        }
        resetDraft()
    }
    function editServer(key,value) { let next=Object.assign({},serverDraft); next[key]=value; serverDraft=next; let pending=Object.assign({},serverChanges); pending[key]=value; serverChanges=pending; saveError="" }
    function resetServer() { serverDraft=Object.assign({},Backend.serverAttributes); serverChanges=({}); saveError="" }
    function saveServer() { if(Backend.setServerAttributes(serverChanges))resetServer(); else saveError="Could not save server attributes." }
    onSelectedKeyChanged: resetServer()
    function paletteColor(value) { let n=Number(value);return n<=0 || n>=216 ? "#000000" : Qt.rgba(Math.floor(n/36)%6/5,Math.floor(n/6)%6/5,n%6/5,1) }
    function openColorPicker(field) {
        if (!inspector.editable || (field!=="lightColor" && field!=="minimapColor")) return
        colorPicker.field=field
        colorPicker.selectedIndex=Math.max(0,Math.min(215,Number(inspector.draft[field]||0)))
        colorPicker.open()
    }
    function placement(value) { edit("isGroundBorder",value===1); edit("isOnBottom",value===2); edit("isOnTop",value===3) }
    function leftFlags() {
        let flags=[["Container","isContainer"],["Stackable","isStackable"],["Force Use","forceUse"],["Multi Use","isUseable"]]
        if(clientVersion>=780 && clientVersion<=854)flags.push(["Chargeable","chargeable"])
        return flags.concat([["Fluid Container","isFluidContainer"],["Fluid","isFluid"],["Unpassable","isUnpassable"],["Unmovable","isUnmoveable"],["Block Missile","blocksMissiles"],["Block Pathfinder","blocksPathfinder"]])
    }
    function rightFlags() {
        let flags=[]
        if(clientVersion>=1010)flags.push(["No Move Animation","noMoveAnimation"])
        flags.push(["Pickupable","isPickupable"])
        if(clientVersion>=755)flags.push(["Hangable","isHangable"],["Hook East","isHorizontal"],["Hook South","isVertical"])
        flags.push(["Rotatable","isRotatable"])
        if(clientVersion>=780)flags.push(["Don't Hide","dontHide"])
        if(clientVersion>=860)flags.push(["Translucent","isTranslucent"])
        flags.push(["Lying Object","isLyingObject"],["Full Ground","fullGround"])
        if(clientVersion>=780)flags.push(["Ignore Look","ignoreLook"])
        if(clientVersion>=710 && clientVersion<=854)flags.push(["Floor Change","floorChange"])
        if(clientVersion>=1021)flags.push(["Useable","usable"])
        if((clientVersion>=710 && clientVersion<=792) || clientVersion>=1092)flags.push(["Wrappable","wrappable"],["Unwrappable","unwrappable"])
        return flags
    }
    Timer {
        interval: {
            let revision=Backend.revision
            let duration=Backend.frameDuration(Backend.category,Number(Backend.details.itemId||0),inspector.isOutfit ? inspector.outfitGroup : 0,inspector.previewFrame)
            return Math.max(1,Math.round((Number(duration.minimum||150)+Number(duration.maximum||150))/2))
        }
        repeat: true
        running: inspector.visible && inspector.animate && (inspector.activeFrameGroup.frames||1)>1
        onTriggered: inspector.previewFrame=(inspector.previewFrame+1)%inspector.activeFrameGroup.frames
    }

    component NumberField: RowLayout {
        id: numberField
        property string caption
        property string field
        property int minimum: 0
        property int maximum: 65535
        property bool available: true
        property bool textureField: false
        property var dataSource: inspector.draft
        spacing: 8
        Layout.alignment: Qt.AlignRight
        Label { text: caption+":"; color: "#b8c7d7"; Layout.preferredWidth: 120; horizontalAlignment: Text.AlignRight }
        SpinBox { from: minimum; to: maximum; value: dataSource[field] ?? minimum; enabled: available && (inspector.editable || (textureField && Backend.loaded && Backend.selected>=0)); editable: true; implicitWidth: 128; implicitHeight: 26; onValueModified: {
            if (textureField && !inspector.isItem) {
                if (Backend.setTextureValue(inspector.outfitGroup,field,value)) {
                    inspector.saveError=""
                    if (field==="frameGroupCount" && inspector.outfitGroup>=value) inspector.outfitGroup=0
                    if (field==="frames" && inspector.previewFrame>=value) inspector.previewFrame=0
                    if (field==="layers" && inspector.outfitLayer>=value) inspector.outfitLayer=0
                } else inspector.saveError="Could not update the selected texture pattern."
            } else inspector.edit(field,value)
        } }
        Rectangle {
            objectName: numberField.field==="lightColor" ? "lightColorSwatch" : "automapColorSwatch"
            visible: numberField.field==="lightColor" || numberField.field==="minimapColor"
            width: 26; height: 20
            color: inspector.paletteColor(inspector.draft[numberField.field]||0)
            border.color: "#718294"
            opacity: numberField.available ? 1 : 0.4
            MouseArea {
                anchors.fill: parent
                enabled: inspector.editable && numberField.available
                cursorShape: Qt.PointingHandCursor
                onClicked: inspector.openColorPicker(numberField.field)
            }
        }
    }
    component ChoiceField: RowLayout {
        id: choiceRow
        property string caption
        property string field
        property var choices
        property bool available: true
        property bool wide: false
        Layout.fillWidth: wide
        Layout.alignment: Qt.AlignRight
        Label { text: choiceRow.caption+":"; color: "#b8c7d7"; Layout.preferredWidth: choiceRow.wide ? 32 : 120; horizontalAlignment: Text.AlignRight }
        ComboBox {
            Layout.fillWidth: choiceRow.wide
            implicitWidth: 170; implicitHeight: 26
            enabled: inspector.editable && choiceRow.available
            textRole: "label"; valueRole: "value"
            model: {
                let entries=choiceRow.choices.map((label,i)=>({label:label,value:i}))
                let value=Number(inspector.draft[choiceRow.field]||0)
                if(value>=entries.length) entries.push({label:String(value),value:value})
                return entries
            }
            currentIndex: { let n=Number(inspector.draft[choiceRow.field]||0);return n<choiceRow.choices.length ? n : model.length-1 }
            onActivated: inspector.edit(choiceRow.field,currentValue)
        }
    }
    component Toggle: CheckBox {
        id: toggleControl
        spacing: 7
        leftPadding: 0; rightPadding: 0
        implicitHeight: 27
        opacity: enabled ? 1 : 0.45
        indicator: Rectangle {
            implicitWidth: 30; implicitHeight: 16
            x: 0; y: Math.round((toggleControl.height-height)/2)
            radius: height/2
            color: toggleControl.checked ? "#168bd0" : "#37414b"
            border.color: toggleControl.checked ? "#4cb6ed" : "#55616c"
            border.width: 1
            Rectangle {
                width: 12; height: 12; radius: 6
                y: 2; x: toggleControl.checked ? parent.width-width-2 : 2
                color: toggleControl.checked ? "#f2f8fc" : "#aab5be"
                border.color: toggleControl.checked ? "#ffffff" : "#c4ccd2"
                Behavior on x { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
            }
        }
        contentItem: Text {
            leftPadding: toggleControl.indicator.width+toggleControl.spacing
            text: toggleControl.text; color: "#d2e4f5"
            font: toggleControl.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
    component Flag: Toggle {
        property string field
        text: ""
        enabled: inspector.editable
        checked: Boolean(inspector.draft[field])
        implicitHeight: 27
        onClicked: inspector.edit(field,checked)
    }
    component PlacementRadio: RadioButton {
        id: placementRadio
        spacing: 7; leftPadding: 0; rightPadding: 0
        implicitHeight: 27
        indicator: Rectangle {
            width: 16; height: 16; radius: 8
            x: 0; y: Math.round((placementRadio.height-height)/2)
            color: "#303943"; border.color: placementRadio.checked ? "#4db4ec" : "#596672"; border.width: 1
            Rectangle {
                anchors.centerIn: parent; width: 8; height: 8; radius: 4
                visible: placementRadio.checked; color: "#e7f5ff"
            }
        }
        contentItem: Text {
            leftPadding: placementRadio.indicator.width+placementRadio.spacing
            text: placementRadio.text; color: "#d2e4f5"; font: placementRadio.font
            verticalAlignment: Text.AlignVCenter
        }
    }
    component PropertyGroup: GroupBox {
        id: group
        property string caption
        property string field
        property bool supported: true
        Layout.fillWidth: true
        topPadding: 27; bottomPadding: 10; leftPadding: 12; rightPadding: 12
        label: Rectangle {
            x: 10; height: 27
            width: groupLabel.implicitWidth+12
            color: "#24292f"
            Flag {
                id: groupLabel
                anchors { left: parent.left; leftMargin: 6; verticalCenter: parent.verticalCenter }
                text: group.caption; field: group.field
                enabled: inspector.editable && group.supported
            }
        }
        background: Rectangle { y: 12; height: parent.height-12; color: "#24292f"; border.color: "#3b424b"; radius: 2 }
    }
    component Section: GroupBox {
        id: section
        Layout.fillWidth: true
        topPadding: 29; bottomPadding: 10; leftPadding: 10; rightPadding: 10
        label: Label { text: section.title.toUpperCase(); color: "#d9e5f0"; font.bold: true; font.pixelSize: 11; leftPadding: 2 }
        background: Item { Rectangle { x: 0; y: 20; width: parent.width; height: 1; color: "#3b4651" } }
    }

    ColumnLayout {
        anchors { fill: parent; bottomMargin: 38 }
        spacing: 8
        RowLayout {
            spacing: 3
            Repeater { model: ["Texture","Properties","Attributes"]
                Tool { required property int index; required property string modelData; objectName: index === 2 ? "serverAttributesTab" : "inspectorTab" + index; text: modelData; visible: index !== 2 || (inspector.isItem && Boolean(Backend.info.otb)); checked: inspector.tabIndex===index; implicitWidth: 96; implicitHeight: 28; onClicked: inspector.tabIndex=index }
            }
            Item { Layout.fillWidth: true }
        }
        StackLayout {
            currentIndex: inspector.tabIndex
            Layout.fillWidth: true; Layout.fillHeight: true
            ScrollView {
                id: textureScroll; clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: textureScroll.availableWidth; spacing: 10
                    Section {
                        title: "Appearance"
                        Layout.preferredHeight: Math.max(220,inspector.height-380)
                        Layout.minimumHeight: appearanceLayout.implicitHeight+40
                        ColumnLayout {
                            id: appearanceLayout; anchors.fill: parent; spacing: 8
                            GridLayout {
                                Layout.fillWidth: true; columns: inspector.width<540 ? 1 : 2
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: "Zoom:"; color: "#b8c7d7" }
                                    Slider { from: 0.5; to: 10; value: inspector.textureZoom; stepSize: 0.1; Layout.preferredWidth: 100; onMoved: inspector.textureZoom=value }
                                    Label { text: inspector.textureZoom.toFixed(1)+"x"; color: "#83cafa" }
                                    Repeater {
                                        model: inspector.isOutfit ? [["↑","North"],["→","East"],["↓","South"],["←","West"]] : []
                                        Tool {
                                            required property int index; required property var modelData
                                            text: modelData[0]; tip: modelData[1]; checked: inspector.outfitDirection===index
                                            implicitWidth: 25; implicitHeight: 25; padding: 2
                                            onClicked: inspector.outfitDirection=index
                                        }
                                    }
                                    Item { Layout.fillWidth: true }
                                    Label { text: inspector.isOutfit ? ["North","East","South","West"][inspector.outfitDirection] : "South"; color: "#91a6ba" }
                                }
                                RowLayout {
                                    Layout.alignment: Qt.AlignRight
                                    Toggle { text: "Edit Pixels"; enabled: Backend.loaded && Backend.selected>=0; checked: inspector.editPixelsMode; onClicked: inspector.editPixelsMode=checked; hoverEnabled: true; ToolTip.visible: hovered; ToolTip.text: "Choose a sprite cell in the preview to paint its pixels." }
                                    Toggle { text: "Film Roll"; checked: inspector.filmVisible; onClicked: inspector.filmVisible=checked }
                                }
                            }
                            Flickable {
                                id: previewFlick
                                Layout.fillWidth: true; Layout.fillHeight: true
                                Layout.minimumHeight: Math.min(520,Math.max(150,previewSurface.height+12))
                                Layout.preferredHeight: Layout.minimumHeight
                                clip: true; boundsBehavior: Flickable.StopAtBounds
                                contentWidth: Math.max(width,previewSurface.width)
                                contentHeight: Math.max(height,previewSurface.height)
                                function centerPreview() {
                                    contentX=Math.max(0,(contentWidth-width)/2)
                                    contentY=Math.max(0,(contentHeight-height)/2)
                                }
                                onContentWidthChanged: Qt.callLater(centerPreview)
                                onContentHeightChanged: Qt.callLater(centerPreview)
                                onWidthChanged: Qt.callLater(centerPreview)
                                onHeightChanged: Qt.callLater(centerPreview)
                                ScrollBar.horizontal: ScrollBar { policy: previewFlick.contentWidth>previewFlick.width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }
                                ScrollBar.vertical: ScrollBar { policy: previewFlick.contentHeight>previewFlick.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }
                                Item {
                                    id: previewSurface
                                    objectName: "patternPreviewSurface"
                                    x: Math.max(0,(previewFlick.width-width)/2)
                                    y: Math.max(0,(previewFlick.height-height)/2)
                                    width: inspector.previewColumns*inspector.patternCellWidth
                                    height: Math.ceil(inspector.shownPatternCount/inspector.previewColumns)*inspector.patternCellHeight
                                    Repeater {
                                        id: patternPreviewRepeater
                                        objectName: "patternPreviewRepeater"
                                        model: inspector.shownPatternCount
                                        delegate: Item {
                                            required property int index
                                            readonly property int sourcePattern: inspector.isOutfit ? inspector.outfitPattern : index
                                            objectName: "patternPreviewTile"
                                            x: (index%inspector.previewColumns)*inspector.patternCellWidth
                                            y: Math.floor(index/inspector.previewColumns)*inspector.patternCellHeight
                                            width: inspector.patternCellWidth
                                            height: inspector.patternCellHeight
                                            Checker { anchors.fill: parent; z: -1 }
                                            Image {
                                                anchors.fill: parent
                                                source: {
                                                    let revision=Backend.revision
                                                    return parent.sourcePattern<inspector.savedPatternCount ? Backend.preview(Backend.selected,inspector.previewFrame,parent.sourcePattern,inspector.outfitGroup,inspector.isOutfit ? inspector.outfitLayer : -1) : ""
                                                }
                                                smooth: false; cache: false
                                            }
                                            MouseArea {
                                                anchors.fill: parent
                                                z: 3
                                                enabled: inspector.editPixelsMode
                                                cursorShape: Qt.CrossCursor
                                                onClicked: function(mouse) {
                                                    const cell=inspector.spriteSize*inspector.textureZoom
                                                    const x=Math.floor(mouse.x/cell)
                                                    const y=Math.floor(mouse.y/cell)
                                                    if(Backend.beginPixelEdit(inspector.outfitGroup,inspector.previewFrame,parent.sourcePattern,
                                                                              inspector.isOutfit ? inspector.outfitLayer : 0,x,y)) {
                                                        inspector.editPixelsMode=false
                                                        pixelEditor.open()
                                                    }
                                                }
                                            }
                                            Rectangle {
                                                anchors.right: parent.right; anchors.bottom: parent.bottom
                                                width: Math.min(parent.width,(inspector.activeFrameGroup.cropSize||32)*inspector.textureZoom)
                                                height: Math.min(parent.height,(inspector.activeFrameGroup.cropSize||32)*inspector.textureZoom)
                                                visible: inspector.cropVisible; color: "transparent"; border.color: "#39d56f"; border.width: 1; z: 2
                                            }
                                        }
                                    }
                                    Canvas {
                                        anchors.fill: parent; visible: inspector.gridVisible; z: 1
                                        onWidthChanged: requestPaint(); onHeightChanged: requestPaint(); onVisibleChanged: requestPaint()
                                        onPaint: {
                                            let c=getContext("2d")
                                            c.clearRect(0,0,width,height)
                                            c.strokeStyle="rgba(131,202,250,0.72)"
                                            c.lineWidth=1
                                            c.beginPath()
                                            let cell=inspector.spriteSize*inspector.textureZoom
                                            let columns=Math.max(1,Math.round(width/cell))
                                            let rows=Math.max(1,Math.round(height/cell))
                                            for(let column=0;column<=columns;++column) {
                                                let x=Math.min(width,column*cell)
                                                c.moveTo(x,0);c.lineTo(x,height)
                                            }
                                            for(let row=0;row<=rows;++row) {
                                                let y=Math.min(height,row*cell)
                                                c.moveTo(0,y);c.lineTo(width,y)
                                            }
                                            c.stroke()
                                        }
                                    }
                                    DropArea {
                                        id: textureDropArea
                                        objectName: "objectTextureDropArea"
                                        anchors.fill: parent
                                        z: 3
                                        enabled: Backend.loaded && Backend.selected >= 0
                                        onEntered: function(drag) {
                                            if (drag.hasUrls || (drag.source && drag.source.spriteId > 0)
                                                    || drag.formats.indexOf("application/x-oteditor-sprite-id") >= 0)
                                                drag.accept(Qt.CopyAction)
                                            else drag.accepted = false
                                        }
                                        onDropped: function(drop) {
                                            const group = inspector.isOutfit ? inspector.outfitGroup : 0
                                            const layer = inspector.isOutfit ? inspector.outfitLayer : 0
                                            const column = Math.floor(drop.x / inspector.patternCellWidth)
                                            const row = Math.floor(drop.y / inspector.patternCellHeight)
                                            const displayedPattern = row * inspector.previewColumns + column
                                            if (displayedPattern < 0 || displayedPattern >= inspector.shownPatternCount) {
                                                drop.accepted = false
                                                return
                                            }
                                            const pattern = inspector.isOutfit ? inspector.outfitPattern : displayedPattern
                                            const width = Math.max(1, Number(inspector.activeFrameGroup.itemWidth || 1))
                                            const height = Math.max(1, Number(inspector.activeFrameGroup.itemHeight || 1))
                                            const tileSize = inspector.spriteSize * inspector.textureZoom
                                            const tileX = width - 1 - Math.floor((drop.x - column * inspector.patternCellWidth) / tileSize)
                                            const tileY = height - 1 - Math.floor((drop.y - row * inspector.patternCellHeight) / tileSize)
                                            let ok = false
                                            if (drop.hasUrls && drop.urls.length)
                                                ok = Backend.importObjectImage(String(drop.urls[0]), group, inspector.previewFrame,
                                                                               pattern, layer, tileX, tileY)
                                            else {
                                                const spriteId = drop.source && drop.source.spriteId > 0
                                                    ? drop.source.spriteId
                                                    : Number(drop.getDataAsString("application/x-oteditor-sprite-id"))
                                                if (spriteId > 0)
                                                    ok = Backend.assignSpriteToCell(group, inspector.previewFrame,
                                                                                    pattern, layer, tileX, tileY, spriteId)
                                            }
                                            if (ok) drop.accept(Qt.CopyAction)
                                            else drop.accepted = false
                                        }
                                    }
                                    Rectangle {
                                        anchors.fill: parent
                                        z: 4
                                        visible: textureDropArea.containsDrag
                                        color: "transparent"
                                        border.color: "#4cb6ed"
                                        border.width: 2
                                    }
                                }
                            }
                            Label { visible: !inspector.isOutfit && inspector.patternCount>inspector.shownPatternCount; text: "Showing first " + inspector.shownPatternCount + " of " + inspector.patternCount + " patterns"; color: "#91a6ba" }
                            RowLayout {
                                visible: Number(inspector.activeFrameGroup.frames||1)>1
                                Layout.alignment: Qt.AlignRight
                                Label { text: "Animations:"; color: "#b8c7d7" }
                                Slider {
                                    from: 0; to: Math.max(0,Number(inspector.activeFrameGroup.frames||1)-1); stepSize: 1
                                    value: inspector.previewFrame; Layout.preferredWidth: 100
                                    onMoved: { inspector.previewFrame=Math.round(value); inspector.animate=false }
                                }
                                Label { text: (inspector.previewFrame+1)+"/"+(inspector.activeFrameGroup.frames||1); color: "#d2e4f5"; Layout.preferredWidth: 40 }
                                Tool { text: inspector.animate ? "Ⅱ" : "▶"; tip: inspector.animate ? "Pause animation" : "Play animation"; implicitWidth: 26; implicitHeight: 24; onClicked: inspector.animate=!inspector.animate }
                            }
                            RowLayout {
                                visible: (inspector.isItem || Backend.category===2) && Boolean(Backend.info.durations) && Number(inspector.activeFrameGroup.frames||1)>1
                                Layout.alignment: Qt.AlignRight
                                Label { text: "Minimum duration ("+inspector.totalDuration("minimum")+" ms):"; color: "#b8c7d7" }
                                SpinBox { id: minimumDuration; objectName: "minimumFrameDuration"; from: 1; to: 60000; value: 100; editable: true; implicitWidth: 128; implicitHeight: 26; onValueModified: inspector.applyFrameDuration("minimum") }
                            }
                            RowLayout {
                                visible: (inspector.isItem || Backend.category===2) && Boolean(Backend.info.durations) && Number(inspector.activeFrameGroup.frames||1)>1
                                Layout.alignment: Qt.AlignRight
                                Label { text: "Maximum duration ("+inspector.totalDuration("maximum")+" ms):"; color: "#b8c7d7" }
                                SpinBox { id: maximumDuration; objectName: "maximumFrameDuration"; from: 1; to: 60000; value: 100; editable: true; implicitWidth: 128; implicitHeight: 26; onValueModified: inspector.applyFrameDuration("maximum") }
                            }
                            Rectangle { Layout.fillWidth: true; height: 1; color: "#3b424b" }
                            ListView {
                                Layout.fillWidth: true; Layout.preferredHeight: 70; visible: inspector.filmVisible; orientation: ListView.Horizontal; spacing: 5; clip: true
                                model: inspector.activeFrameGroup.frames||1
                                delegate: Rectangle {
                                    required property int index
                                    width: 62; height: 66; color: inspector.previewFrame===index ? "#174e73" : "#2b3138"; border.color: "#46505b"
                                    Image { anchors.horizontalCenter: parent.horizontalCenter; y: 3; width: 44; height: 44; fillMode: Image.PreserveAspectFit; smooth: false; source: { let rev=Backend.revision; return Backend.preview(Backend.selected,index,inspector.outfitPattern,inspector.outfitGroup,inspector.isOutfit ? inspector.outfitLayer : -1) } }
                                    Label { anchors.horizontalCenter: parent.horizontalCenter; y: 48; text: index+1; font.pixelSize: 11 }
                                    MouseArea { anchors.fill: parent; onClicked: { inspector.previewFrame=index; inspector.animate=false } }
                                }
                                ScrollBar.horizontal: ScrollBar {}
                            }
                            RowLayout {
                                visible: inspector.isOutfit && Number(inspector.draft.frameGroupCount||1)>1
                                Layout.alignment: Qt.AlignRight
                                Label { text: "Group:"; color: "#b8c7d7" }
                                Slider {
                                    from: 0; to: Math.max(0,Number(inspector.draft.frameGroupCount||1)-1); stepSize: 1
                                    value: inspector.outfitGroup; Layout.preferredWidth: 100
                                    onMoved: { inspector.outfitGroup=Math.round(value); inspector.outfitLayer=0; inspector.previewFrame=0 }
                                }
                                Label { text: inspector.outfitGroup===0 ? "Idle / Stand" : "Walking"; color: "#d2e4f5"; Layout.preferredWidth: 75 }
                            }
                            RowLayout {
                                visible: inspector.isOutfit && Number(inspector.activeFrameGroup.layers||1)>1
                                Layout.alignment: Qt.AlignRight
                                Label { text: "Layer:"; color: "#b8c7d7" }
                                Slider {
                                    from: 0; to: Math.max(0,Number(inspector.activeFrameGroup.layers||1)-1); stepSize: 1
                                    value: inspector.outfitLayer; Layout.preferredWidth: 100
                                    onMoved: inspector.outfitLayer=Math.round(value)
                                }
                                Label { text: (inspector.outfitLayer+1)+"/"+(inspector.activeFrameGroup.layers||1); color: "#d2e4f5"; Layout.preferredWidth: 75 }
                            }
                            Toggle { text: "Show Crop Size"; implicitHeight: 24; Layout.maximumHeight: 24; checked: inspector.cropVisible; onClicked: inspector.cropVisible=checked }
                            Toggle { text: "Show Grid"; implicitHeight: 24; Layout.maximumHeight: 24; checked: inspector.gridVisible; onClicked: inspector.gridVisible=checked }
                        }
                    }
                    Section {
                        title: "Has Bones"
                        visible: inspector.isOutfit && inspector.clientVersion >= 780
                        ColumnLayout { anchors.left: parent.left; anchors.right: parent.right
                            Toggle {
                                text: "Has Bones"
                                checked: Boolean(Backend.details.hasBones)
                                onClicked: Backend.setOutfitBones(checked,boneDirection.currentIndex,boneX.value,boneY.value)
                            }
                            RowLayout {
                                Label { text: "Direction"; Layout.preferredWidth: 110 }
                                ComboBox { id: boneDirection; model: ["North","South","East","West"]; Layout.fillWidth: true }
                            }
                            RowLayout {
                                enabled: Boolean(Backend.details.hasBones)
                                Label { text: "Offset X"; Layout.preferredWidth: 110 }
                                SpinBox {
                                    id: boneX
                                    from: -32768; to: 32767; editable: true; Layout.fillWidth: true
                                    value: Number((Backend.details.boneOffsetX || [0,0,0,0])[boneDirection.currentIndex] || 0)
                                    onValueModified: Backend.setOutfitBones(true,boneDirection.currentIndex,value,boneY.value)
                                }
                            }
                            RowLayout {
                                enabled: Boolean(Backend.details.hasBones)
                                Label { text: "Offset Y"; Layout.preferredWidth: 110 }
                                SpinBox {
                                    id: boneY
                                    from: -32768; to: 32767; editable: true; Layout.fillWidth: true
                                    value: Number((Backend.details.boneOffsetY || [0,0,0,0])[boneDirection.currentIndex] || 0)
                                    onValueModified: Backend.setOutfitBones(true,boneDirection.currentIndex,boneX.value,value)
                                }
                            }
                        }
                    }
                    Section {
                        title: "Pattern"
                        ColumnLayout { anchors.left: parent.left; anchors.right: parent.right; spacing: 3
                            Repeater {
                                model: (inspector.isOutfit && Boolean(Backend.info.groups) ? [["Frame Groups","frameGroupCount",1,2]] : []).concat([["Width","itemWidth",1,32],["Height","itemHeight",1,32],["Crop Size","cropSize",1,255],["Layers","layers",1,16],["Pattern X","patternX",1,32],["Pattern Y","patternY",1,32],["Pattern Z","patternZ",1,32],["Animations","frames",1,255]])
                                NumberField { required property var modelData; caption: modelData[0]; field: modelData[1]; minimum: modelData[2]; maximum: modelData[3]; textureField: true; dataSource: field==="frameGroupCount" ? inspector.draft : inspector.activeFrameGroup; available: field!=="cropSize" || dataSource.itemWidth>1 || dataSource.itemHeight>1 }
                            }
                        }
                    }
                }
            }
            ScrollView {
                id: propertiesScroll; enabled: Backend.loaded && Backend.selected>=0; clip: true; contentWidth: availableWidth
                ColumnLayout {
                    width: propertiesScroll.availableWidth; spacing: 10
                    PropertyGroup { caption: "Is Ground"; field: "isGround"; visible: inspector.isItem
                        ColumnLayout { anchors.left: parent.left; anchors.right: parent.right
                            NumberField { caption: "Ground Speed"; field: "groundSpeed"; available: Boolean(inspector.draft.isGround) }
                        }
                    }
                    PropertyGroup { caption: "Has Light"; field: "hasLight"
                        ColumnLayout { anchors.left: parent.left; anchors.right: parent.right; spacing: 3
                            NumberField { caption: "Light Color"; field: "lightColor"; available: Boolean(inspector.draft.hasLight) }
                            NumberField { caption: "Light Intensity"; field: "lightLevel"; available: Boolean(inspector.draft.hasLight) }
                        }
                    }
                    PropertyGroup { caption: "Automap"; field: "hasMinimapColor"; visible: inspector.isItem
                        ColumnLayout { anchors.left: parent.left; anchors.right: parent.right
                            NumberField { caption: "Automap Color"; field: "minimapColor"; available: Boolean(inspector.draft.hasMinimapColor) }
                        }
                    }
                    PropertyGroup { caption: "Has Offset"; field: "hasOffset"
                        ColumnLayout { anchors.left: parent.left; anchors.right: parent.right; spacing: 3
                            NumberField { caption: "Offset X"; field: "offsetX"; minimum: -32768; maximum: 32767; available: Boolean(inspector.draft.hasOffset) }
                            NumberField { caption: "Offset Y"; field: "offsetY"; minimum: -32768; maximum: 32767; available: Boolean(inspector.draft.hasOffset) }
                        }
                    }
                    PropertyGroup { caption: "Has Elevation"; field: "hasElevation"; visible: inspector.isItem
                        ColumnLayout { anchors.left: parent.left; anchors.right: parent.right
                            NumberField { caption: "Elevation"; field: "elevation"; available: Boolean(inspector.draft.hasElevation) }
                        }
                    }
                    PropertyGroup { caption: "Equip"; field: "hasCloth"; visible: inspector.isItem && inspector.clientVersion>=900
                        ColumnLayout { anchors.left: parent.left; anchors.right: parent.right
                            ChoiceField { caption: "Slot"; field: "clothSlot"; wide: true; available: Boolean(inspector.draft.hasCloth); choices: ["Two Hand Weapon","Head","Neck","Backpack","Body","Right Hand","Left Hand","Legs","Feet","Finger","Ammo","Purse"] }
                        }
                    }
                    PropertyGroup { caption: "Market"; field: "hasMarket"; visible: inspector.isItem && inspector.clientVersion>=940
                        ColumnLayout { anchors.left: parent.left; anchors.right: parent.right; spacing: 3
                            RowLayout { Layout.fillWidth: true
                                Label { text: "Name:"; color: "#b8c7d7" }
                                TextField { Layout.fillWidth: true; text: inspector.draft.marketName||""; enabled: Boolean(inspector.draft.hasMarket); onTextEdited: inspector.edit("marketName",text) }
                            }
                            ChoiceField { caption: "Category"; field: "marketCategory"; available: Boolean(inspector.draft.hasMarket); choices: ["All","Armors","Amulets","Boots","Containers","Decoration","Food","Helmets and Hats","Legs","Others","Potions","Rings","Runes","Shields","Tools","Valuables","Ammunition","Axes","Clubs","Distance Weapons","Swords","Wands and Rods","Premium Scrolls","Tibia Coins","Creature Products"] }
                            Repeater { model: [["Trade As","marketTradeAs"],["Show As","marketShowAs"],["Vocation","marketVocation"],["Level","marketLevel"]]
                                NumberField { required property var modelData; caption: modelData[0]; field: modelData[1]; available: Boolean(inspector.draft.hasMarket) }
                            }
                        }
                    }
                    Section { title: "Write / Read"; visible: inspector.isItem
                        ColumnLayout { anchors.left: parent.left; anchors.right: parent.right
                            RowLayout { Layout.alignment: Qt.AlignRight
                                Toggle { text: "Writable"; checked: Boolean(inspector.draft.isWritable) && !inspector.draft.writableOnce; onClicked: { inspector.edit("isWritable",checked); inspector.edit("writableOnce",false) } }
                                NumberField { caption: "Max Length"; field: "maxTextLength"; available: Boolean(inspector.draft.isWritable) && !inspector.draft.writableOnce }
                            }
                            RowLayout { Layout.alignment: Qt.AlignRight
                                Toggle { text: "Writable Once"; checked: Boolean(inspector.draft.isWritable) && Boolean(inspector.draft.writableOnce); onClicked: { inspector.edit("isWritable",checked); inspector.edit("writableOnce",checked) } }
                                NumberField { caption: "Max Length"; field: "maxTextLength"; available: Boolean(inspector.draft.isWritable) && Boolean(inspector.draft.writableOnce) }
                            }
                        }
                    }
                    PropertyGroup { caption: "Has Action"; field: "hasAction"; visible: inspector.isItem && inspector.clientVersion>=1021
                        ColumnLayout { anchors.left: parent.left; anchors.right: parent.right
                            ChoiceField { caption: "Action Type"; field: "defaultAction"; available: Boolean(inspector.draft.hasAction); choices: ["None","1","2","3","4"] }
                        }
                    }
                    Section { visible: inspector.isItem; label: Toggle { text: "Lens Help"; checked: (inspector.draft.lensHelp||0)>0; onClicked: inspector.edit("lensHelp",checked ? 1 : 0) }
                        ColumnLayout { anchors.left: parent.left; anchors.right: parent.right
                            NumberField { caption: "Lens Help"; field: "lensHelp"; available: (inspector.draft.lensHelp||0)>0 }
                        }
                    }
                    Section { title: "Flags"; visible: Backend.category!==3
                        RowLayout { anchors.left: parent.left; anchors.right: parent.right; spacing: 20
                            ColumnLayout { Layout.fillWidth: true; spacing: 0; visible: inspector.isItem
                                ButtonGroup { id: placementGroup }
                                Repeater { model: inspector.clientVersion>=755 ? [["Common",0],["Ground Border",1],["Bottom",2],["Top",3]] : [["Common",0],["Bottom",2],["Top",3]]
                                    PlacementRadio { required property var modelData; text: modelData[0]; ButtonGroup.group: placementGroup; checked: modelData[1]===(inspector.draft.isOnTop ? 3 : inspector.draft.isOnBottom ? 2 : inspector.draft.isGroundBorder ? 1 : 0); onClicked: inspector.placement(modelData[1]) }
                                }
                                Repeater { model: inspector.leftFlags()
                                    Flag { required property var modelData; text: modelData[0]; field: modelData[1] }
                                }
                            }
                            ColumnLayout { Layout.fillWidth: true; spacing: 0; visible: inspector.isItem
                                Repeater { model: inspector.rightFlags()
                                    Flag { required property var modelData; text: modelData[0]; field: modelData[1] }
                                }
                            }
                            Flag { visible: inspector.isOutfit; text: "Animate Always"; field: "animateAlways"; Layout.alignment: Qt.AlignTop }
                        }
                    }
                }
            }
            ScrollView {
                id: serverScroll; clip: true; contentWidth: availableWidth
                ColumnLayout {
                    width: serverScroll.availableWidth; spacing: 10
                    Label { text: Backend.info.otb ? (Backend.serverId>=0 ? "Server ID: "+Backend.serverId+"  ·  Client ID: "+(inspector.draft.itemId||"") : "No OTB entry for this item") : "No items.otb in this project"; color: "#a9bdcf"; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    Button { text: "Create items.otb"; visible: Backend.loaded && inspector.isItem && !Boolean(Backend.info.otb); onClicked: Backend.createOtbFile() }
                    Button { text: "Create server item"; visible: inspector.isItem && Boolean(Backend.info.otb) && Backend.serverId<0; enabled: inspector.editable && Number(inspector.draft.itemId||0)<=65535; onClicked: Backend.createServerItem() }
                    Section { title: "Identity"; visible: Backend.serverId>=0
                        ColumnLayout { anchors.left: parent.left; anchors.right: parent.right
                            Repeater { model: [["Server ID","serverId",1],["Client ID","clientId",100]]
                                RowLayout { required property var modelData; Layout.fillWidth: true
                                    Label { text: modelData[0]; Layout.preferredWidth: 125 }
                                    SpinBox { Layout.fillWidth: true; from: modelData[2]; to: 65535; value: Number(inspector.serverDraft[modelData[1]]||modelData[2]); editable: true; onValueModified: inspector.editServer(modelData[1],value) }
                                }
                            }
                            Repeater { model: [["Name","name"],["Description","description"]]
                                RowLayout { required property var modelData; Layout.fillWidth: true
                                    Label { text: modelData[0]; Layout.preferredWidth: 125 }
                                    TextField { Layout.fillWidth: true; text: String(inspector.serverDraft[modelData[1]] ?? ""); onTextEdited: inspector.editServer(modelData[1],text) }
                                }
                            }
                            Label {
                                visible: String(inspector.serverDraft.nameSource || "").length > 0
                                text: "Name source: " + inspector.serverDraft.nameSource
                                      + " · server ID " + Backend.serverId
                                color: "#91a8bd"
                                Layout.fillWidth: true
                                ToolTip.visible: nameSourceArea.containsMouse
                                ToolTip.text: inspector.serverDraft.nameSource === "items.xml"
                                              ? String(Backend.info.itemsXmlPath || "")
                                              : String(Backend.info.otbPath || "")
                                MouseArea { id: nameSourceArea; anchors.fill: parent; hoverEnabled: true }
                            }
                            RowLayout { Layout.fillWidth: true
                                Label { text: "Group"; Layout.preferredWidth: 125 }
                                ComboBox { Layout.fillWidth: true; model: ["None","Ground","Container","Weapon","Ammunition","Armor","Changes","Teleport","Magic Field","Writeable","Key","Splash","Fluid","Door","Deprecated","Podium"]; currentIndex: Number(inspector.serverDraft.groupId||0); onActivated: inspector.editServer("groupId",currentIndex) }
                            }
                        }
                    }
                    Section { title: "Values"; visible: Backend.serverId>=0
                        ColumnLayout { anchors.left: parent.left; anchors.right: parent.right
                            Repeater { model: [["Ground speed","speed",0,65535],["Read/write length","maxReadWriteLength",0,65535],["Read length","maxReadLength",0,65535],["Minimap color","minimapColor",0,65535],["Ware ID","wareId",0,65535],["Light level","lightLevel",0,255],["Light color","lightColor",0,255],["Stack order","stackOrder",-128,127]]
                                RowLayout { required property var modelData; Layout.fillWidth: true
                                    Label { text: modelData[0]; Layout.fillWidth: true }
                                    SpinBox { from: modelData[2]; to: modelData[3]; value: Number(inspector.serverDraft[modelData[1]]||0); editable: true; onValueModified: inspector.editServer(modelData[1],value) }
                                }
                            }
                        }
                    }
                    Section { title: "Flags"; visible: Backend.serverId>=0
                        GridLayout { anchors.left: parent.left; anchors.right: parent.right; columns: width<440 ? 1 : 2
                            Repeater { model: [["Unpassable","unpassable"],["Block missiles","blockMissiles"],["Block pathfinder","blockPathfinder"],["Elevation","hasElevation"],["Useable","useable"],["Pickupable","pickupable"],["Moveable","moveable"],["Stackable","stackable"],["Always on top","alwaysOnTop"],["Readable","readable"],["Rotatable","rotatable"],["Hangable","hangable"],["Hook east","hookEast"],["Hook south","hookSouth"],["Distance read","allowDistRead"],["Client duration","clientDuration"],["Client charges","clientCharges"],["Ignore look","ignoreLook"],["Animation","animation"],["Full ground","fullGround"],["Force use","forceUse"]]
                                CheckBox { required property var modelData; text: modelData[0]; checked: Boolean(inspector.serverDraft[modelData[1]]); onClicked: inspector.editServer(modelData[1],checked) }
                            }
                        }
                    }
                }
            }
        }
        Label { visible: inspector.saveError.length>0; text: inspector.saveError; color: "#e7b467"; wrapMode: Text.Wrap; Layout.fillWidth: true }
    }
    Item {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 34
        implicitHeight: 34
        RowLayout {
            anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
            Tool { objectName: "inspectorResetButton"; text: "Reset"; implicitWidth: 72; enabled: inspector.tabIndex===2 ? Object.keys(inspector.serverChanges).length>0 : Object.keys(inspector.changes).length>0 || Backend.textureEditPending; onClicked: inspector.tabIndex===2 ? inspector.resetServer() : inspector.resetDraft() }
            Tool { objectName: "inspectorSaveButton"; text: "Save"; accent: true; implicitWidth: 72; enabled: inspector.tabIndex===2 ? Backend.serverId>=0 && Object.keys(inspector.serverChanges).length>0 : (inspector.editable && Object.keys(inspector.changes).length>0) || Backend.textureEditPending; onClicked: inspector.tabIndex===2 ? inspector.saveServer() : inspector.saveDraft() }
        }
    }
    PaletteColorDialog {
        id: colorPicker
        objectName: "paletteColorPicker"
        owner: inspector
        anchors.centerIn: parent
    }
    PixelEditorDialog { id: pixelEditor; backend: Backend; anchors.centerIn: parent }
}
