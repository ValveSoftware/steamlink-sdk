import QtQuick 2.0

Rectangle {
    id: root

    property int current: grid.currentIndex
    property bool showHeader: false
    property bool showFooter: false
    property int currentItemChangedCount: 0

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
            }
        }
    ]

    Component {
        id: headerFooter
        Rectangle { height: 30; width: 240; color: "blue" }
    }

    GridView {
        id: grid
        objectName: "grid"
        focus: true
        width: 240
        height: 320
        currentIndex: 35
        cellWidth: 80
        cellHeight: 60
        cacheBuffer: 0
        delegate: myDelegate
        highlightMoveDuration: 400
        model: testModel
        header: root.showHeader ? headerFooter : null
        footer: root.showFooter ? headerFooter : null

        onCurrentItemChanged: { root.currentItemChangedCount++ }
    }
}
