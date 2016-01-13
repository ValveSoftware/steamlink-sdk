import QtQuick 1.0

Rectangle {
    id: root
    property real delegateHeight: 20
    width: 240
    height: 320
    color: "#ffffff"
    resources: [
        Component {
            id: myDelegate
            Rectangle {
                id: wrapper
                objectName: "wrapper"
                height: root.delegateHeight
                Behavior on height { NumberAnimation {} }
                width: 240
                Text {
                    text: index
                }
                Text {
                    x: 30
                    objectName: "displayText"
                    text: display
                }
                Text {
                    x: 200
                    text: wrapper.y
                }
                color: ListView.isCurrentItem ? "lightsteelblue" : "white"
            }
        },
        Component {
            id: myHighlight
            Rectangle { color: "green" }
        }
    ]
    ListView {
        id: list
        objectName: "list"
        focus: true
        width: 240
        height: 320
        model: testModel
        delegate: myDelegate
        highlight: myHighlight
        highlightMoveSpeed: 1000
        highlightResizeSpeed: 1000
    }
}
