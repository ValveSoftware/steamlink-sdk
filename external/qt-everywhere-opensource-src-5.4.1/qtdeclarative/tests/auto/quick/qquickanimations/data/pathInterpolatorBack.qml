import QtQuick 2.0

PathInterpolator {
    path: Path {
        startX: 50; startY: 50
        PathLine { x: 50; y: 100 }
        PathLine { x: 100; y: 100 }
        PathLine { x: 100; y: 50 }
        PathLine { x: 200; y: 50 }
    }
}
