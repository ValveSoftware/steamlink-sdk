import QtQuick 2.0

GridView {
    id: gridView
    width: 400; height: 400
    cellHeight: 100
    cellWidth: 100
    model: 32

    topMargin: 50
    snapMode: GridView.SnapToRow
    preferredHighlightBegin: 50
    preferredHighlightEnd: 50
    highlightRangeMode: GridView.ApplyRange

    delegate: Rectangle {
        width: 100
        height: 100
        color: index % 2 == 0 ? "#FFFF00" : "#0000FF"
    }

    highlight: Rectangle { color: "red"; z: 2 }
    highlightMoveDuration: 1000
}
