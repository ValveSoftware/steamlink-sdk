import QtQuick 2.0

MultiPointTouchArea {
    width: 240
    height: 320

    property int touchCount: 0
    property int cancelCount: 0

    minimumTouchPoints: 1
    maximumTouchPoints: 4
    touchPoints: [
        TouchPoint { id: p1; objectName: "point1" },
        TouchPoint { objectName: "point2" }
    ]

    onPressed: { touchCount = touchPoints.length }
    onTouchUpdated: { touchCount = touchPoints.length }
    onCanceled: { cancelCount = touchPoints.length }

    Rectangle {
        color: "red"
        height: 30
        width: 30
        x: p1.x
        y: p1.y
    }

}
