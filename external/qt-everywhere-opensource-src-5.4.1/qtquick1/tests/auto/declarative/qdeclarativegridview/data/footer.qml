import QtQuick 1.0

Rectangle {
    function changeFooter() {
        grid.footer = footer2
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
        width: 240
        height: 320
        cellWidth: 80
        cellHeight: 60
        model: testModel
        delegate: myDelegate
        footer: Text { objectName: "footer"; text: "Footer"; height: 30 }
    }

    Component {
        id: footer2
        Text { objectName: "footer2"; text: "Footer 2"; height: 20 }
    }
}
