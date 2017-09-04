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

        MultiPointTouchArea {
            scale: 0.5
            anchors.fill: parent
            maximumTouchPoints: 5
            objectName: "touchArea"

            property int pointCount: 0

            onPressed: pointCount = touchPoints.length;
            onTouchUpdated: pointCount = touchPoints.length;
        }
    }
}
