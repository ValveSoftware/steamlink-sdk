import QtQuick 2.0

Rectangle {
    property bool showHeader: false

    function changeFooter() {
        list.footer = footer2
    }
    width: 240
    height: 320
    color: "#ffffff"
    Component {
        id: myDelegate
        Rectangle {
            id: wrapper
            objectName: "wrapper"
            height: 20
            width: 40
            Text {
                text: index + " " + x + "," + y
            }
            color: ListView.isCurrentItem ? "lightsteelblue" : "white"
        }
    }
    Component {
        id: headerComponent
        Text { objectName: "header"; text: "Header " + x + "," + y; width: 100; height: 30 }
    }

    ListView {
        id: list
        objectName: "list"
        focus: true
        width: 240
        height: 320
        model: testModel
        delegate: myDelegate
        header: parent.showHeader ? headerComponent : null
        footer: Text { objectName: "footer"; text: "Footer " + x + "," + y; width: 100; height: 30 }
    }

    Component {
        id: footer2
        Text { objectName: "footer2"; text: "Footer 2 " + x + "," + y; width: 50; height: 20 }
    }
}
