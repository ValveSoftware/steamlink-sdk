import QtQuick 1.0

Rectangle {
    width: 240
    height: 320
    color: "#ffffff"
    resources: [
        Component {
            id: myDelegate
            Rectangle {
                id: wrapper
                objectName: "wrapper"
                width: 80
                height: 60
                border.color: "blue"
                Text {
                    text: index
                }
                Text {
                    y: 20
                    id: displayText
                    objectName: "displayText"
                    text: display
                }
                color: GridView.isCurrentItem ? "lightsteelblue" : "white"
            }
        }
    ]
    GridView {
        id: grid
        objectName: "grid"
        width: 240
        height: 320
        cellWidth: 80
        cellHeight: 60
        model: testModel
        delegate: myDelegate
    }
}
