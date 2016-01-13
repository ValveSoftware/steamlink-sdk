import QtQuick 2.0

ListView {
    id: myList

    width: 400; height: 400
    model: 10

    header: Item {
        height: parent ? 20 : 10
        width: 400
    }

    delegate: Rectangle {
        width: parent.width; height: 20
        color: index % 2 ? "green" : "red"
    }

    Component.onCompleted: myList.verticalLayoutDirection = ListView.BottomToTop
}
