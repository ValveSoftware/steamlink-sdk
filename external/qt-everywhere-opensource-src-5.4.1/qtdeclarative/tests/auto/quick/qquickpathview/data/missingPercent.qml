import QtQuick 2.0

Path {
    startY: 0
    startX: 0
    PathLine { x: 0; y: 50 }
    PathPercent { value: .5 }
    PathLine { x: 50; y: 50 }
}
