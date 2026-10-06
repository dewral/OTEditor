import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: slicer
    objectName: "slicerDialog"
    title: "Slicer"
    width: 800; height: 600
    anchors.centerIn: parent
    modal: true
    padding: 0
    standardButtons: Dialog.NoButton
    background: Rectangle { color: "#202426"; border.color: "#343a3e"; radius: 3 }
    header: Rectangle {
        implicitHeight: 32; color: "#282d30"; border.color: "#343a3e"
        Label { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; text: "Slicer"; font.bold: true; color: "#e5eaf1" }
    }
    footer: Rectangle {
        implicitHeight: 40; color: "#282d30"; border.color: "#343a3e"
        Tool { anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; width: 88; text: "Close"; onClicked: slicer.close() }
    }
    property var backend
    property int clientSpriteSize: 32
    property real zoom: 1
    property string sourcePath: ""
    readonly property int tilePixels: [32,64,128,256][dimension.currentIndex] || 32

    component SlicerToggle: CheckBox {
        id: toggle
        spacing: 4; leftPadding: 0; rightPadding: 0
        implicitHeight: 20
        indicator: Rectangle {
            implicitWidth: 24; implicitHeight: 12
            x: 0; y: (toggle.height-height)/2; radius: 6
            color: toggle.checked ? "#399ee8" : "#363d41"
            border.color: toggle.checked ? "#6db9ef" : "#707a81"
            Rectangle {
                width: 10; height: 10; radius: 5; y: 1
                x: toggle.checked ? parent.width-width-1 : 1
                color: toggle.checked ? "#f2f8fc" : "#aab5be"; border.color: "#c4ccd2"
            }
        }
        contentItem: Text {
            leftPadding: toggle.indicator.width+toggle.spacing
            text: toggle.text; color: toggle.enabled ? "#e0e5e8" : "#77828e"
            font: toggle.font; verticalAlignment: Text.AlignVCenter
        }
    }
    component SlicerSpin: SpinBox {
        id: spin
        implicitHeight: 22; leftPadding: 18; rightPadding: 18
        font.pixelSize: 11
        background: Rectangle { color: "#1c2022"; border.color: "#3c4449" }
        contentItem: TextInput {
            text: spin.textFromValue(spin.value,spin.locale)
            color: "#e0e5e8"; font: spin.font
            horizontalAlignment: TextInput.AlignHCenter
            verticalAlignment: TextInput.AlignVCenter
            readOnly: !spin.editable; validator: spin.validator
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            selectByMouse: true
        }
        down.indicator: Rectangle {
            x: 0; y: 0; width: 18; height: spin.height
            color: spin.down.pressed ? "#465564" : "#2b3033"
            border.color: "#3c4449"
            Text { anchors.centerIn: parent; text: "−"; color: spin.value>spin.from ? "#e0e5e8" : "#77828e"; font.pixelSize: 12 }
        }
        up.indicator: Rectangle {
            x: spin.width-width; y: 0; width: 18; height: spin.height
            color: spin.up.pressed ? "#465564" : "#2b3033"
            border.color: "#3c4449"
            Text { anchors.centerIn: parent; text: "+"; color: spin.value<spin.to ? "#e0e5e8" : "#77828e"; font.pixelSize: 12 }
        }
    }

    function resetSelection() {
        offsetX.value=0; offsetY.value=0; columns.value=1; rows.value=1
    }
    function clampSelection() {
        if(!backend) return
        columns.value=Math.min(columns.value,Math.max(1,Math.floor(backend.slicerWidth/tilePixels)))
        rows.value=Math.min(rows.value,Math.max(1,Math.floor(backend.slicerHeight/tilePixels)))
        offsetX.value=Math.min(offsetX.value,Math.max(0,backend.slicerWidth-columns.value*tilePixels))
        offsetY.value=Math.min(offsetY.value,Math.max(0,backend.slicerHeight-rows.value*tilePixels))
    }
    function moveSelection(dx,dy) {
        offsetX.value=Math.max(0,Math.min(offsetX.to,offsetX.value+dx))
        offsetY.value=Math.max(0,Math.min(offsetY.to,offsetY.value+dy))
    }
    function openImage(url) {
        if(backend && backend.slicerOpen(url)) {
            sourcePath=backend.localPath(url)
            resetSelection()
            return true
        }
        return false
    }

    FileDialog {
        id: sourceDialog; title: "Open image"
        nameFilters: ["Images (*.png *.bmp *.jpg *.jpeg *.gif *.webp)"]
        onAccepted: slicer.openImage(selectedFile.toString())
    }
    Shortcut { sequence: "Ctrl+O"; enabled: slicer.visible; onActivated: sourceDialog.open() }
    Shortcut { sequence: "Left"; enabled: slicer.visible && backend.slicerWidth>0; onActivated: slicer.moveSelection(-1,0) }
    Shortcut { sequence: "Right"; enabled: slicer.visible && backend.slicerWidth>0; onActivated: slicer.moveSelection(1,0) }
    Shortcut { sequence: "Up"; enabled: slicer.visible && backend.slicerWidth>0; onActivated: slicer.moveSelection(0,-1) }
    Shortcut { sequence: "Down"; enabled: slicer.visible && backend.slicerWidth>0; onActivated: slicer.moveSelection(0,1) }
    Shortcut { sequence: "Ctrl++"; enabled: slicer.visible; onActivated: zoomControl.value=Math.min(5,zoomControl.value+0.1) }
    Shortcut { sequence: "Ctrl+-"; enabled: slicer.visible; onActivated: zoomControl.value=Math.max(0.1,zoomControl.value-0.1) }

    ColumnLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 40; color: "#282d30"
            border.color: "#343a3e"
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 6; spacing: 3
                Tool { text: "▱"; Layout.preferredWidth: 32; tip: "Open image (Ctrl+O)"; onClicked: sourceDialog.open() }
                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 23; color: "#343a3e" }
                Tool { text: "↻"; Layout.preferredWidth: 28; enabled: backend.slicerWidth>0; tip: "Rotate right 90°"; onClicked: { backend.slicerTransform("rotateRight"); slicer.clampSelection() } }
                Tool { text: "↺"; Layout.preferredWidth: 28; enabled: backend.slicerWidth>0; tip: "Rotate left 90°"; onClicked: { backend.slicerTransform("rotateLeft"); slicer.clampSelection() } }
                Tool { text: "↕"; Layout.preferredWidth: 28; enabled: backend.slicerWidth>0; tip: "Flip vertically"; onClicked: backend.slicerTransform("flipVertical") }
                Tool { text: "↔"; Layout.preferredWidth: 28; enabled: backend.slicerWidth>0; tip: "Flip horizontally"; onClicked: backend.slicerTransform("flipHorizontal") }
                Item { Layout.fillWidth: true }
            }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.margins: 6; spacing: 6
            Rectangle {
                Layout.preferredWidth: 145; Layout.fillHeight: true
                color: "#202426"; border.color: "#343a3e"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 5; spacing: 4
                    Item { Layout.fillHeight: true }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 54; color: "#202426"; border.color: "#343a3e"
                        ColumnLayout { anchors.fill: parent; anchors.margins: 5; spacing: 1
                            Label { text: "Sprite Dimension" }
                            ComboBox {
                                id: dimension; objectName: "slicerDimension"; Layout.fillWidth: true; Layout.preferredHeight: 25
                                model: ["32×32", "64×64", "128×128", "256×256"]
                                Component.onCompleted: currentIndex=Math.max(0,[32,64,128,256].indexOf(slicer.clientSpriteSize))
                                onActivated: slicer.clampSelection()
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 174; color: "#202426"; border.color: "#343a3e"
                        ColumnLayout { anchors.fill: parent; anchors.margins: 5; spacing: 2
                            Label { text: "Cells" }
                            SlicerToggle { id: subdivisions; text: "Subdivisions"; Layout.fillWidth: true; Layout.preferredHeight: 21 }
                            SlicerToggle { id: emptySprites; text: "Empty Sprites"; Layout.fillWidth: true; Layout.preferredHeight: 21 }
                            Item { Layout.preferredHeight: 5 }
                            RowLayout { Layout.fillWidth: true; spacing: 3
                                Label { text: "X:"; Layout.preferredWidth: 49; horizontalAlignment: Text.AlignRight }
                                SlicerSpin { id: offsetX; Layout.fillWidth: true; Layout.preferredHeight: 22; editable: true; from: 0; to: Math.max(0,backend.slicerWidth-columns.value*slicer.tilePixels) }
                            }
                            RowLayout { Layout.fillWidth: true; spacing: 3
                                Label { text: "Y:"; Layout.preferredWidth: 49; horizontalAlignment: Text.AlignRight }
                                SlicerSpin { id: offsetY; Layout.fillWidth: true; Layout.preferredHeight: 22; editable: true; from: 0; to: Math.max(0,backend.slicerHeight-rows.value*slicer.tilePixels) }
                            }
                            RowLayout { Layout.fillWidth: true; spacing: 3
                                Label { text: "Columns:"; Layout.preferredWidth: 49; horizontalAlignment: Text.AlignRight }
                                SlicerSpin { id: columns; Layout.fillWidth: true; Layout.preferredHeight: 22; editable: true; from: 1; to: Math.max(1,Math.min(20,Math.floor((backend.slicerWidth-offsetX.value)/slicer.tilePixels))); value: 1; onValueChanged: slicer.clampSelection() }
                            }
                            RowLayout { Layout.fillWidth: true; spacing: 3
                                Label { text: "Rows:"; Layout.preferredWidth: 49; horizontalAlignment: Text.AlignRight }
                                SlicerSpin { id: rows; Layout.fillWidth: true; Layout.preferredHeight: 22; editable: true; from: 1; to: Math.max(1,Math.min(20,Math.floor((backend.slicerHeight-offsetY.value)/slicer.tilePixels))); value: 1; onValueChanged: slicer.clampSelection() }
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 43; color: "#202426"; border.color: "#343a3e"
                        ColumnLayout { anchors.fill: parent; anchors.margins: 4; spacing: 0
                            Label { text: "Zoom  " + Math.round(slicer.zoom*100) + "%" }
                            Slider { id: zoomControl; Layout.fillWidth: true; Layout.preferredHeight: 20; from: 0.1; to: 5; value: 1; onValueChanged: slicer.zoom=value }
                        }
                    }
                    Button {
                        text: "Crop"; Layout.fillWidth: true; Layout.preferredHeight: 26
                        enabled: backend.slicerWidth>=slicer.tilePixels && backend.slicerHeight>=slicer.tilePixels
                        onClicked: backend.slicerCut(offsetX.value,offsetY.value,columns.value,rows.value,slicer.tilePixels,emptySprites.checked)
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true; color: "#202426"; border.color: "#343a3e"
                Rectangle { x: 17; y: 13; width: parent.width-23; height: 1; color: "#151819" }
                Canvas {
                    id: topRuler; x: 17; y: 2; width: parent.width-23; height: 14
                    onWidthChanged: requestPaint()
                    onPaint: {
                        const ctx=getContext("2d"); ctx.clearRect(0,0,width,height)
                        ctx.fillStyle="#282d30"; ctx.fillRect(0,0,width,height)
                        ctx.strokeStyle="#94a9bd"; ctx.fillStyle="#b9cadd"; ctx.font="8px sans-serif"
                        for(let i=0;i<width;i+=8) { const n=Math.round((i+imageViewport.contentX)/slicer.zoom); ctx.beginPath(); ctx.moveTo(i,14); ctx.lineTo(i,n%32===0?4:9); ctx.stroke(); if(n%64===0)ctx.fillText(n,i+2,7) }
                    }
                    Connections { target: imageViewport; function onContentXChanged() { topRuler.requestPaint() } }
                    Connections { target: slicer; function onZoomChanged() { topRuler.requestPaint() } }
                }
                Canvas {
                    id: leftRuler; x: 2; y: 17; width: 14; height: parent.height-23
                    onHeightChanged: requestPaint()
                    onPaint: {
                        const ctx=getContext("2d"); ctx.clearRect(0,0,width,height)
                        ctx.fillStyle="#282d30"; ctx.fillRect(0,0,width,height)
                        ctx.strokeStyle="#94a9bd"
                        for(let i=0;i<height;i+=8) { const n=Math.round((i+imageViewport.contentY)/slicer.zoom); ctx.beginPath(); ctx.moveTo(14,i); ctx.lineTo(n%32===0?4:9,i); ctx.stroke() }
                    }
                    Connections { target: imageViewport; function onContentYChanged() { leftRuler.requestPaint() } }
                    Connections { target: slicer; function onZoomChanged() { leftRuler.requestPaint() } }
                }
                Flickable {
                    id: imageViewport; x: 17; y: 17; width: parent.width-23; height: parent.height-23
                    clip: true; contentWidth: Math.max(width,imageLayer.width); contentHeight: Math.max(height,imageLayer.height)
                    Canvas { anchors.fill: parent
                        onWidthChanged: requestPaint(); onHeightChanged: requestPaint()
                        onPaint: {
                            const ctx=getContext("2d")
                            for(let y=0;y<height;y+=8)for(let x=0;x<width;x+=8) {
                                ctx.fillStyle=((x+y)/8)%2===0?"#29313a":"#313b45"
                                ctx.fillRect(x,y,8,8)
                            }
                        }
                    }
                    Item {
                        id: imageLayer; width: backend.slicerWidth*slicer.zoom; height: backend.slicerHeight*slicer.zoom
                        Image { anchors.fill: parent; source: backend.slicerWidth ? "image://itempreview/slicer/source?v="+backend.slicerRevision : ""; fillMode: Image.Stretch; smooth: false; cache: false }
                        Rectangle {
                            visible: backend.slicerWidth>0; x: offsetX.value*slicer.zoom; y: offsetY.value*slicer.zoom
                            width: columns.value*slicer.tilePixels*slicer.zoom; height: rows.value*slicer.tilePixels*slicer.zoom
                            color: "transparent"; border.color: "#bcd4e3"; border.width: 1
                            Repeater { model: subdivisions.checked ? columns.value-1 : 0
                                Rectangle { x: (index+1)*slicer.tilePixels*slicer.zoom; width: 1; height: parent.height; color: "#a9bac4" } }
                            Repeater { model: subdivisions.checked ? rows.value-1 : 0
                                Rectangle { y: (index+1)*slicer.tilePixels*slicer.zoom; height: 1; width: parent.width; color: "#a9bac4" } }
                        }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.CrossCursor
                            property real pressX: 0; property real pressY: 0
                            property int oldX: 0; property int oldY: 0
                            onPressed: mouse => {
                                pressX=mouse.x; pressY=mouse.y; oldX=offsetX.value; oldY=offsetY.value
                                if(mouse.x<oldX*slicer.zoom || mouse.x>(oldX+columns.value*slicer.tilePixels)*slicer.zoom ||
                                   mouse.y<oldY*slicer.zoom || mouse.y>(oldY+rows.value*slicer.tilePixels)*slicer.zoom) {
                                    offsetX.value=Math.max(0,Math.min(offsetX.to,Math.round(mouse.x/slicer.zoom-columns.value*slicer.tilePixels/2)))
                                    offsetY.value=Math.max(0,Math.min(offsetY.to,Math.round(mouse.y/slicer.zoom-rows.value*slicer.tilePixels/2)))
                                    oldX=offsetX.value; oldY=offsetY.value
                                }
                            }
                            onPositionChanged: mouse => { if(pressed) {
                                offsetX.value=Math.max(0,Math.min(offsetX.to,oldX+Math.round((mouse.x-pressX)/slicer.zoom)))
                                offsetY.value=Math.max(0,Math.min(offsetY.to,oldY+Math.round((mouse.y-pressY)/slicer.zoom)))
                            } }
                        }
                    }
                    WheelHandler { onWheel: event => {
                        if(event.modifiers & Qt.ControlModifier) {
                            zoomControl.value=Math.max(0.1,Math.min(5,zoomControl.value+(event.angleDelta.y>0?0.1:-0.1)))
                            event.accepted=true
                        }
                    } }
                }
                DropArea {
                    x: 17; y: 17; width: parent.width-23; height: parent.height-23
                    onDropped: drop => { if(drop.urls.length && slicer.openImage(drop.urls[0]))drop.acceptProposedAction() }
                }
            }
            Rectangle {
                Layout.preferredWidth: 145; Layout.fillHeight: true; color: "#202426"; border.color: "#343a3e"
                ColumnLayout { anchors.fill: parent; anchors.margins: 5; spacing: 4
                    Label { text: "Sprites ("+backend.slicerCount+")" }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                        model: backend.slicerCount; spacing: 2
                        delegate: Rectangle {
                            width: ListView.view.width; height: 38
                            color: index%2 ? "#283039" : "#2c3540"; border.color: "#343a3e"
                            Row { anchors.verticalCenter: parent.verticalCenter; spacing: 5
                                Image { width: 34; height: 34; fillMode: Image.PreserveAspectFit; smooth: false; source: "image://itempreview/slicer/tile/"+index+"?v="+backend.slicerRevision }
                                Label { text: String(index+1); anchors.verticalCenter: parent.verticalCenter }
                            }
                        }
                    }
                    Button { text: "Import"; Layout.fillWidth: true; Layout.preferredHeight: 24; enabled: backend.loaded && backend.slicerCount>0 && slicer.tilePixels===slicer.clientSpriteSize; onClicked: backend.slicerImport() }
                    Button { text: "Clear"; Layout.fillWidth: true; Layout.preferredHeight: 24; enabled: backend.slicerCount>0; onClicked: backend.slicerClear() }
                }
            }
        }
    }
}
