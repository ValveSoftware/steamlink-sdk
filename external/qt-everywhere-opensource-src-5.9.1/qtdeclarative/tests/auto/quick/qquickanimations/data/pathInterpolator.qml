import QtQuick 2.0

PathInterpolator {
    path: Path {
        startX: 50; startY: 50
        PathCubic {
            x: 300; y: 300

            control1X: 300; control1Y: 50
            control2X: 50; control2Y: 300
        }
    }
}
