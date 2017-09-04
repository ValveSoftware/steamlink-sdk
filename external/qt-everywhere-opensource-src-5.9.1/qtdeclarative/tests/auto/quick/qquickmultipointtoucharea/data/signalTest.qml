import QtQuick 2.0

MultiPointTouchArea {
    width: 240
    height: 320

    function clearCounts() {
        touchPointPressCount = 0;
        touchPointUpdateCount = 0;
        touchPointReleaseCount = 0;
        touchCount = 0;
        touchUpdatedHandled = false;
    }

    property int touchPointPressCount: 0
    property int touchPointUpdateCount: 0
    property int touchPointReleaseCount: 0
    property int touchCount: 0
    property bool touchUpdatedHandled: false

    maximumTouchPoints: 5

    onPressed: { touchPointPressCount = touchPoints.length }
    onUpdated: { touchPointUpdateCount = touchPoints.length }
    onReleased: { touchPointReleaseCount = touchPoints.length }
    onTouchUpdated: {
        touchCount = touchPoints.length
        touchUpdatedHandled = true
    }
}
