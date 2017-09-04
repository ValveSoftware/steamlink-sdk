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
        path: Path {
            //no startX/Y defined (should automatically start from redRects pos)
            PathCubic {
                x: 300; y: 300

                control1X: 300; control1Y: 50
                control2X: 50; control2Y: 300
            }
        }
    }
}
