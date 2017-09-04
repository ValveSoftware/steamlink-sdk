import QtQuick 2.0

Path {
    startX: 120; startY: 100

    PathAttribute { name: "scale"; value: 1.0 }
    PathQuad { x: 120; y: 25; controlX: 260; controlY: 75 }
    PathPercent { value: 0.3 }
    PathLine { x: 120; y: 100 }
    PathCubic {
        x: 180; y: 0; control1X: -10; control1Y: 90
        control2X: 210; control2Y: 90
    }
}
