import QtQuick 2.0

Rectangle {
    width: 240
    height: 320

    MultiPointTouchArea {
        width: parent.width
        height: 160
        minimumTouchPoints: 2
        maximumTouchPoints: 2
        onGestureStarted: gesture.grab()
        touchPoints: [
            TouchPoint { objectName: "point11" },
            TouchPoint { objectName: "point12" }
        ]
    }

    MultiPointTouchArea {
        width: parent.width
        height: 160
        y: 160
        minimumTouchPoints: 3
        maximumTouchPoints: 3
        onGestureStarted: gesture.grab()
        touchPoints: [
            TouchPoint { objectName: "point21" },
            TouchPoint { objectName: "point22" },
            TouchPoint { objectName: "point23" }
        ]
    }
}
