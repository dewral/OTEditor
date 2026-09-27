import QtQuick
Rectangle {
    color: "#1d2228"; clip: true
    Canvas {
        anchors.fill: parent
        onWidthChanged: requestPaint(); onHeightChanged: requestPaint()
        onPaint: {
            let ctx = getContext("2d")
            ctx.fillStyle = "#20252b"; ctx.fillRect(0, 0, width, height); ctx.fillStyle = "#282d33"
            for (let y=0;y<height;y+=12) for (let x=0;x<width;x+=12) if ((x/12+y/12)%2===0) ctx.fillRect(x,y,12,12)
        }
    }
    Rectangle { anchors.fill: parent; color: "transparent"; border.color: "#11171d" }
}

