import QtQuick 1.0

Rectangle {
    property int current: list.currentIndex
    width: 240
    height: 320
    color: "#ffffff"
    resources: [
        Component {
            id: myDelegate
            Rectangle {
                id: wrapper
                objectName: "wrapper"
                height: 20
                width: 240
                Text {
                    text: index
                }
                Text {
                    x: 30
                    id: textName
                    objectName: "textName"
                    text: name
                }
                Text {
                    x: 120
                    id: textNumber
                    objectName: "textNumber"
                    text: number
                }
                Text {
                    x: 200
                    text: wrapper.y
                }
                color: ListView.isCurrentItem ? "lightsteelblue" : "white"
            }
        }
    ]
    ListView {
        id: list
        objectName: "list"
        focus: true
        currentIndex: -1
        width: 240
        height: 320
        delegate: myDelegate
        highlightMoveSpeed: 1000
        model: testModel
    }
}
