import QtQuick 1.0
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
        opacity: (whiteRect.width-blackRect.x+whiteRect.height-blackRect.y-199)/200
        Text { text: blackRect.opacity}
        MouseArea {
            objectName: "mouseregion"
            anchors.fill: parent
            drag.target: haveTarget ? blackRect : undefined
            drag.axis: Drag.XandYAxis
            drag.minimumX: 0
            drag.maximumX: whiteRect.width-blackRect.width
            drag.minimumY: 0
            drag.maximumY: whiteRect.height-blackRect.height
         }
     }
 }
