import QtQuick 2.1
Rectangle {
    ListModel {
        id: model
        ListElement { name: "hello"}
    }
    ListView {
        id: list
        model: model
        delegate: Item {
        }
    }
    function remove() {
        model.remove(0)
        list.forceLayout()
        isCurrentItemNull = list.currentItem === null //check no seg fault
    }

    function add() {
        model.append({name: "hello"})
        list.forceLayout()
        isCurrentItemNull = list.currentItem === null
    }
    property bool isCurrentItemNull
}
