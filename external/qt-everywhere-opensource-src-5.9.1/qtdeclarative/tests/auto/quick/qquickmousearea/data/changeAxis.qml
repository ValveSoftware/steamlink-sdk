import QtQuick 2.0
Rectangle {
    id: whiteRect
    width: 200
    height: 200
    color: "white"
    Rectangle {
        id: blackRect
        objectName: "blackrect"
        color: "black"
        y: 50
        x: 50
        width: 100
        height: 100
        MouseArea {
            objectName: "mouseregion"
            anchors.fill: parent
            drag.target: blackRect
            drag.axis: blackRect.x <= 75 ? Drag.XAndYAxis : Drag.YAxis
         }
     }
 }
