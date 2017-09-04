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
        duration: 1000;
        path: Path {
            startX: 100; startY: 100

            PathSvg {
                path: "M 200 200"
            }
        }
    }
}
