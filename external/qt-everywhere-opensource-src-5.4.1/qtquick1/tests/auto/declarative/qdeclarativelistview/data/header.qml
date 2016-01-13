import QtQuick 1.0

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
                text: index
            }
            color: ListView.isCurrentItem ? "lightsteelblue" : "white"
        }
    }
    ListView {
        id: list
        objectName: "list"
        focus: true
        width: 240
        height: 320
        snapMode: ListView.SnapToItem
        model: testModel
        delegate: myDelegate
        header: Text { objectName: "header"; text: "Header"; height: 20 }
    }
    Component {
        id: header2
        Text { objectName: "header2"; text: "Header 2"; height: 10 }
    }
}
