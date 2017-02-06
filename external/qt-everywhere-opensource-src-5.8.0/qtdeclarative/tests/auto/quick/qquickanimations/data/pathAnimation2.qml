import QtQuick 2.0

Rectangle {
    width: 400
    height: 400

    Rectangle {
        id: redRect
        color: "red"
        width: 100; height: 100
        x: 50; y: 50
    }

    PathAnimation {
        target: redRect
        duration: 100;
        endRotation: 0
        orientationEntryDuration: 10
        orientationExitDuration: 10
        orientation: PathAnimation.RightFirst
        path: Path {
            startX: 50; startY: 50
            PathLine { x: 300; y: 300 }
        }
    }
}
