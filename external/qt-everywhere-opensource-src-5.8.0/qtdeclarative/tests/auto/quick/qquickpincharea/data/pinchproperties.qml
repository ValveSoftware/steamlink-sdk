import QtQuick 2.0
Rectangle {
    id: whiteRect
    property variant center
    property real scale: -1.0
    property int pointCount: 0
    property bool pinchActive: false
    width: 240; height: 320
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
        PinchArea {
            id: pincharea
            objectName: "pincharea"
            anchors.fill: parent
            pinch.target: blackRect
            pinch.dragAxis: Drag.XAndYAxis
            pinch.minimumX: 0
            pinch.maximumX: whiteRect.width-blackRect.width
            pinch.minimumY: 0
            pinch.maximumY: whiteRect.height-blackRect.height
            pinch.minimumScale: 1.0
            pinch.maximumScale: 2.0
            pinch.minimumRotation: 0.0
            pinch.maximumRotation: 90.0
            onPinchStarted: {
                whiteRect.center = pinch.center
                whiteRect.scale = pinch.scale
                whiteRect.pointCount = pinch.pointCount;
                whiteRect.pinchActive = true;
            }
            onPinchUpdated: {
                whiteRect.center = pinch.center
                whiteRect.scale = pinch.scale
                whiteRect.pointCount = pinch.pointCount;
            }
            onPinchFinished: {
                whiteRect.center = pinch.center
                whiteRect.scale = pinch.scale
                whiteRect.pointCount = pinch.pointCount;
                whiteRect.pinchActive = false;
            }
         }
     }
 }
