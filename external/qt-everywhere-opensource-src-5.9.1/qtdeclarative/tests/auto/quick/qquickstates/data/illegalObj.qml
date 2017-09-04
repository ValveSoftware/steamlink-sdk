import QtQuick 2.0

Rectangle {
    id: myItem

    states : State {
        PropertyChanges {
            target: myItem
            children: Item { id: newItem }
        }
    }
}
