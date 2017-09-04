import QtQuick 2.0

PathView {
    width: 400
    height: 200
    model: 100
    pathItemCount: 20
    path: Path {
        startX: 0; startY: 100
        PathLine { x: 400; y: 100 }
    }
    delegate: Rectangle { objectName: "wrapper"; height: 100; width: 2; color: PathView.isCurrentItem?"red" : "black" }
    dragMargin: 100
    preferredHighlightBegin: 0.5
    preferredHighlightEnd: 0.5
    Text {
        objectName: "text"
        text: "current index: " + parent.currentIndex
    }
}
