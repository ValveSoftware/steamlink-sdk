import QtQuick 2.0

Rectangle {
    function changeHeader() {
        grid.header = header2
    }
    width: 240
    height: 320
    color: "#ffffff"
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
            color: GridView.isCurrentItem ? "lightsteelblue" : "white"
        }
    }
    GridView {
        id: grid
        objectName: "grid"
        width: initialViewWidth
        height: initialViewHeight
        cellWidth: 80
        cellHeight: 60
        model: testModel
        delegate: myDelegate
        header: Text { objectName: "header"; text: "Header " + x + "," + y; width: 100; height: 30 }
    }

    Component {
        id: header2
        Text { objectName: "header2"; text: "Header 2 " + x + "," + y; width: 50; height: 20 }
    }
}
