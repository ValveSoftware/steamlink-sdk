import QtQuick 2.0

Item {
    id: root
    property Item parentItem: null
    Repeater {
        model: 1
        Item {
            Component.onCompleted: root.parentItem = parent
        }
    }
}
