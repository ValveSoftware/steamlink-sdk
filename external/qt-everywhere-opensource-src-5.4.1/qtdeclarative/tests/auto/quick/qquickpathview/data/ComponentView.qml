import QtQuick 2.0

PathView {
    id: view

    property string title

    width: 100; height: 100;

    model: 1
    delegate: Text { objectName: "listItem"; text: view.title }

    path: Path {
        startX: 25; startY: 25;
        PathLine { x: 75; y: 75 }
    }
}
