import QtQuick 2.0

PathInterpolator {
    path: Path {
        startX: 200; startY: 280
        PathCurve { x: 150; y: 280 }
        PathCurve { x: 150; y: 80 }
        PathCurve { x: 0; y: 80 }
    }
}
