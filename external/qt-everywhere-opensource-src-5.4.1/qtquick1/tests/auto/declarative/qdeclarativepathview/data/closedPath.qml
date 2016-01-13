import QtQuick 1.0

Path {
    startY: 120
    startX: 160
    PathQuad {
        y: 120
        x: 80
        controlY: 330
        controlX: 100
    }
    PathLine {
        y: 160
        x: 20
    }
    PathCubic {
        y: 120
        x: 160
        control1Y: 0
        control1X: 100
        control2Y: 000
        control2X: 200
    }
}
