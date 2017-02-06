import QtQuick 2.0

MultiPointTouchArea {
    width: 240
    height: 320

    minimumTouchPoints: 1
    maximumTouchPoints: 5
    touchPoints: [
        TouchPoint { objectName: "point1" },
        TouchPoint { objectName: "point2" },
        TouchPoint { objectName: "point3" },
        TouchPoint { objectName: "point4" },
        TouchPoint { objectName: "point5" }
    ]

    function clearCounts() {
        touchPointPressCount = 0;
        touchPointUpdateCount = 0;
        touchPointReleaseCount = 0;
        touchPointCancelCount = 0;
        touchCount = 0;
        touchUpdatedHandled = false;
    }

    property int touchPointPressCount: 0
    property int touchPointUpdateCount: 0
    property int touchPointReleaseCount: 0
    property int touchPointCancelCount: 0
    property int touchCount: 0
    property bool touchUpdatedHandled: false

    onPressed: { touchPointPressCount = touchPoints.length }
    onUpdated: { touchPointUpdateCount = touchPoints.length }
    onReleased: { touchPointReleaseCount = touchPoints.length }
    onCanceled: { touchPointCancelCount = touchPoints.length }
    onTouchUpdated: {
        touchCount = touchPoints.length
        touchUpdatedHandled = true
    }
}
