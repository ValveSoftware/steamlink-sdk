import QtQuick 2.0

MultiPointTouchArea {
    width: 240
    height: 320

    minimumTouchPoints: 1
    maximumTouchPoints: 4
    touchPoints: [
        TouchPoint { objectName: "point1" },
        TouchPoint { objectName: "point2" },
        TouchPoint { objectName: "point3" },
        TouchPoint { objectName: "point4" }
    ]
}
