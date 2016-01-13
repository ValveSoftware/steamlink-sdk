import QtQuick 1.0

Item {
    id: root

    property bool test1: root.children.length == 3
    property bool test2: root.children[0] == item1
    property bool test3: root.children[1] == item2
    property bool test4: root.children[2] == item3
    property bool test5: root.children[3] == null

    children: [ Item { id: item1 }, Item { id: item2 }, Item { id: item3 } ]
}

