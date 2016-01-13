import QtQuick 2.0

Rectangle {
    property bool showHeader: false

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
    Component {
        id: headerComponent
        Text { objectName: "header"; text: "Header " + x + "," + y; width: 100; height: 30 }
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
        header: parent.showHeader ? headerComponent : null
        footer: Text { objectName: "footer"; text: "Footer " + x + "," + y; width: 100; height: 30 }
    }

    Component {
        id: footer2
        Text { objectName: "footer2"; text: "Footer 2" + x + "," + y; width: 50; height: 20 }
    }
}
