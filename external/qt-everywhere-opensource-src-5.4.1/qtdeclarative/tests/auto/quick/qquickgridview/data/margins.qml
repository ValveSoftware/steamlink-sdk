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
            width: 100
            height: 80
            border.color: "blue"
            property string name: model.name
            Text {
                text: index
            }
            Text {
                x: 40
                text: wrapper.x + ", " + wrapper.y
            }
            Text {
                y: 20
                id: textName
                objectName: "textName"
                text: name
            }
            Text {
                y: 40
                id: textNumber
                objectName: "textNumber"
                text: number
            }
            color: GridView.isCurrentItem ? "lightsteelblue" : "white"
        }
    }
    GridView {
        id: grid
        objectName: "grid"
        width: 240
        height: 320
        cellWidth: 100
        cellHeight: 80
        cacheBuffer: 0
        leftMargin: 30
        rightMargin: 50
        flow: GridView.TopToBottom
        layoutDirection: (testRightToLeft == true) ? Qt.RightToLeft : Qt.LeftToRight
        model: testModel
        delegate: myDelegate
    }
    Text { anchors.bottom: parent.bottom; text: grid.contentX }
}
