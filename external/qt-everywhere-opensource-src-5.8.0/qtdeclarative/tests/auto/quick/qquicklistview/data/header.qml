import QtQuick 2.0

Rectangle {
    function changeHeader() {
        list.header = header2
    }
    width: 240
    height: 320
    color: "#ffffff"
    Component {
        id: myDelegate
        Rectangle {
            id: wrapper
            objectName: "wrapper"
            height: 30
            width: 240
            Text {
                text: index + " " + parent.x + "," + parent.y
            }
            color: ListView.isCurrentItem ? "lightsteelblue" : "white"
        }
    }
    ListView {
        id: list
        objectName: "list"
        focus: true
        width: initialViewWidth
        height: initialViewHeight
        snapMode: ListView.SnapToItem
        model: testModel
        delegate: myDelegate
        header: Text { objectName: "header"; text: "Header " + x + "," + y; width: 100; height: 30 }
    }
    Component {
        id: header2
        Text { objectName: "header2"; text: "Header " + x + "," + y; width: 50; height: 20 }
    }

}
