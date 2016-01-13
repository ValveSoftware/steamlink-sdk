import QtQuick 1.0

PathView {
    id: pathView
    width: 240; height: 200
    path: Path {
        startX: pathView.undef/2.0; startY: 0
        PathLine { x: pathView.undef/2.0; y: 0 }
    }

    delegate: Text { text: value }
    model: ListModel {
        ListElement { value: "one" }
        ListElement { value: "two" }
        ListElement { value: "three" }
    }
}
