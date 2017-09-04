import QtQuick 2.0

Rectangle {
    id: root
    width: 240
    height: 240
    color: "#ffffff"

    Component {
        id: myDelegate
        Rectangle {
            id: wrapper
            objectName: "wrapper"
            height: 80
            width: 80
            Column {
                Text {
                    text: index
                }
                Text {
                    text: wrapper.x + ", " + wrapper.y
                }
            }
            color: GridView.isCurrentItem ? "lightsteelblue" : "transparent"
        }
    }
    GridView {
        id: grid
        objectName: "grid"
        anchors.fill: parent
        cellWidth: 80
        cellHeight: 80
        preferredHighlightBegin: 20
        preferredHighlightEnd: 100
        snapMode: GridView.SnapToRow
        layoutDirection: Qt.RightToLeft
        flow: GridView.TopToBottom
        highlightRangeMode: GridView.StrictlyEnforceRange
        highlight: Rectangle { width: 80; height: 80; color: "yellow" }
        model: 39
        delegate: myDelegate
    }

    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        text: grid.contentX + ", " + grid.contentY
    }
}
