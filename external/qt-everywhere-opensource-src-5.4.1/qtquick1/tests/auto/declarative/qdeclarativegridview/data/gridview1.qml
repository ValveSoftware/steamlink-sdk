import QtQuick 1.1

Rectangle {
    id: root
    property int count: grid.count
    property bool showHeader: false
    property bool showFooter: false
    property int added: -1
    property variant removed

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
                GridView.onAdd: root.added = index
                GridView.onRemove: root.removed = name
            }
        },
        Component {
            id: headerFooter
            Rectangle { width: 30; height: 320; color: "blue" }
        }
    ]
    GridView {
        id: grid
        objectName: "grid"
        width: 240
        height: 320
        cellWidth: 80
        cellHeight: 60
        flow: (testTopToBottom == false) ? GridView.LeftToRight : GridView.TopToBottom
        layoutDirection: (testRightToLeft == true) ? Qt.RightToLeft : Qt.LeftToRight
        model: testModel
        delegate: myDelegate
        header: root.showHeader ? headerFooter : null
        footer: root.showFooter ? headerFooter : null
    }
}
