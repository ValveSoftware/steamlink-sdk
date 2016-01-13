import QtQuick 2.0

Rectangle {
    id: root
    width: 240
    height: 320
    color: "#ffffff"

    Component {
        id: myDelegate
        Rectangle {
            id: wrapper
            objectName: "wrapper"
            height: column.height
            Column {
                id: column
                Text {
                    text: "index: " + index + ", delegate A"
                    Component.onCompleted: height = index % 2 ? 30 : 20
                }
                Text {
                    x: 200
                    text: wrapper.y
                    height: 25
                }
            }
            color: ListView.isCurrentItem ? "lightsteelblue" : "#EEEEEE"
        }
    }
    ListView {
        id: list
        objectName: "list"
        focus: true
        width: 240
        height: 320
        model: 2
        delegate: myDelegate
        highlightMoveVelocity: 1000
        highlightResizeVelocity: 1000
        cacheBuffer: 400
    }
    Text { anchors.bottom: parent.bottom; text: list.contentY }
}
