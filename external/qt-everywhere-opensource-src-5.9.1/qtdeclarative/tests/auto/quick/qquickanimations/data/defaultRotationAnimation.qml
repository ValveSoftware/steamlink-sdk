import QtQuick 2.0

Rectangle {
    id: rect

    RotationAnimation {
        target: rect
        from: 0
        to: 360
        running: true
        duration: 1000
        loops: Animation.Infinite
    }
}
