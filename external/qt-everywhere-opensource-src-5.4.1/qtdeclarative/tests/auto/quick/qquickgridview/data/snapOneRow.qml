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
            height: 120
            width: 120
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
        cellWidth: 120
        cellHeight: 120
        preferredHighlightBegin: 20
        preferredHighlightEnd: 140
        snapMode: GridView.SnapOneRow
        layoutDirection: Qt.RightToLeft
        flow: GridView.TopToBottom
        highlightRangeMode: GridView.StrictlyEnforceRange
        highlight: Rectangle { width: 120; height: 120; color: "yellow" }
        model: 8
        delegate: myDelegate
    }

    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        text: grid.contentX + ", " + grid.contentY
    }
}
