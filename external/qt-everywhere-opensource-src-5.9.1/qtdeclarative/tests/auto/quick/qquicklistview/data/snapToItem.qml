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
            color: ListView.isCurrentItem ? "lightsteelblue" : "transparent"
        }
    }
    ListView {
        id: list
        objectName: "list"
        anchors.fill: parent
//        preferredHighlightBegin: 20
//        preferredHighlightEnd: 100
        preferredHighlightBegin: 20
        preferredHighlightEnd: 100
        snapMode: ListView.SnapToItem
        orientation: ListView.Horizontal
        layoutDirection: Qt.RightToLeft
        highlightRangeMode: ListView.StrictlyEnforceRange
        highlight: Rectangle { width: 80; height: 80; color: "yellow" }
        model: 10
        delegate: myDelegate
    }

    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        text: list.contentX + ", " + list.contentY
    }
}
