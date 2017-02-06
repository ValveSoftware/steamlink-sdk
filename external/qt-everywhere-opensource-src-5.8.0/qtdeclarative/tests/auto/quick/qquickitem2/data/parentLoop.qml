import QtQuick 2.0

Item {
    Item {
        id: item1
        objectName: "item1"

        Item {
            id: item2
            objectName: "item2"
        }
    }
    Component.onCompleted: item1.parent = item2
}
