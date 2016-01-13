import QtQuick 1.0

Item {
    id: root

    property bool test1
    property bool test2
    property bool test3
    property bool test4
    property bool test5

    Component.onCompleted: {
        test1 = (root.resources.length >= 3)
        test2 = root.resources[0] == item1
        test3 = root.resources[1] == item2
        test4 = root.resources[2] == item3
        test5 = root.resources[10] == null
    }

    resources: [ Item { id: item1 }, Item { id: item2 }, Item { id: item3 } ]
}
