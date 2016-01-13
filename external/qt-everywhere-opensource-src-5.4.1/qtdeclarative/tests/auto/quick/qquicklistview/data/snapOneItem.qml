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
            height: 200
            width: 200
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
        preferredHighlightBegin: 20
        preferredHighlightEnd: 220
        snapMode: ListView.SnapOneItem
        orientation: ListView.Horizontal
        layoutDirection: Qt.RightToLeft
        highlightRangeMode: ListView.StrictlyEnforceRange
//        highlightRangeMode: ListView.NoHighlightRange
        highlight: Rectangle { width: 200; height: 200; color: "yellow" }
        flickDeceleration: 200 // encourages long flick
        model: 4
        delegate: myDelegate
    }

    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        text: list.contentX + ", " + list.contentY
    }
}
