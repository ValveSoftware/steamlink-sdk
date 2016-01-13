import QtQuick 2.0

Rectangle {
    width: 400
    height: 400

    Rectangle {
        x: 100
        y: 100
        width: 200
        height: 200
        rotation: 45

        Rectangle {
            id: rect
            scale: 0.5
            color: "black"
            anchors.fill: parent

            PinchArea {
                anchors.fill: parent
                objectName: "pinchArea"

                property bool pinching: false

                pinch.target: rect
                pinch.dragAxis: Drag.XAndYAxis
                onPinchStarted: pinching = true
                onPinchFinished: pinching = false
            }
        }
    }
}
