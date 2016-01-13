import QtQuick 2.0

MultiPointTouchArea {
    width: 240
    height: 320

    minimumTouchPoints: 2
    maximumTouchPoints: 4
    touchPoints: [
        TouchPoint {},
        TouchPoint {},
        TouchPoint {},
        TouchPoint {}
    ]
}
