import QtQuick 2.0

ListView {
    width: 100
    height: 100

    delegate: Item {
        width: 100
        height: 10
    }
    model: listModel

    ListModel {
        id: listModel

        ListElement { label: "a" }
        ListElement { label: "b" }
        ListElement { label: "c" }
        ListElement { label: "d" }
        ListElement { label: "e" }
        ListElement { label: "f" }
    }
}
