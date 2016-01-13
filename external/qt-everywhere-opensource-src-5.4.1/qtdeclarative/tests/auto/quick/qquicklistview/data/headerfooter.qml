import QtQuick 2.0

ListView {
    id: view

    width: 240
    height: 320

    model: testModel

    header: Rectangle {
        objectName: "header"
        width: orientation == ListView.Horizontal ? 20 : view.width
        height: orientation == ListView.Horizontal ? view.height : 20
        color: "red"
    }
    footer: Rectangle {
        objectName: "footer"
        width: orientation == ListView.Horizontal ? 30 : view.width
        height: orientation == ListView.Horizontal ? view.height : 30
        color: "blue"
    }

    delegate: Rectangle {
        width: view.width
        height: 30
        Text { text: index + "(" + x + ")" }
    }
}
