import QtQuick 1.0

Rectangle {
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
        model: testModel
        delegate: myDelegate
        footer: Text { objectName: "footer"; text: "Footer"; height: 30 }
    }

    Component {
        id: footer2
        Text { objectName: "footer2"; text: "Footer 2"; height: 20 }
    }
}
