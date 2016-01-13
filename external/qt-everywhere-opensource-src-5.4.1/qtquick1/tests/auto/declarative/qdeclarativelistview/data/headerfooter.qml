import QtQuick 1.1

ListView {
    id: view
    property bool horizontal: false
    property bool rtl: false
    width: 240
    height: 320

    orientation: horizontal ? ListView.Horizontal : ListView.Vertical
    header: Rectangle {
        objectName: "header"
        width: horizontal ? 20 : view.width
        height: horizontal ? view.height : 20
        color: "red"
    }
    footer: Rectangle {
        objectName: "footer"
        width: horizontal ? 30 : view.width
        height: horizontal ? view.height : 30
        color: "blue"
    }
//    model: testModel
    delegate: Text { width: 30; height: 30; text: index + "(" + x + ")" }
    layoutDirection: rtl ? Qt.RightToLeft : Qt.LeftToRight
}
