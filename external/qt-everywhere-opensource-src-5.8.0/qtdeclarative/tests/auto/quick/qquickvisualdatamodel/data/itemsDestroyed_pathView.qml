import QtQuick 2.0

PathView {
    width: 100
    height: 100

    model: myModel
    delegate: Item {
        objectName: "delegate"
        width: 100
        height: 20
    }

    path: Path {
        startX: 50; startY: 0
        PathLine { x: 50; y: 100 }
    }
}
