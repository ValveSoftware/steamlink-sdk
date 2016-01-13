import QtQuick 2.0


Rectangle{
    id: tr
    property alias lbl: txt.text
    property alias hTileMode: img.horizontalTileMode
    property alias vTileMode: img.verticalTileMode
    property alias xPos: tr.x
    property int yPos: tr.y
    property alias smoothing: img.smooth

    BorderImage{ id: img; source: "../shared/world.png"; width: 70; height: 70; }
    Text{
        id: txt
        text: "default"
        anchors.top: img.bottom
        anchors.horizontalCenter: img.horizontalCenter
        font.family: "Arial"
        font.pointSize: 8
    }
}
