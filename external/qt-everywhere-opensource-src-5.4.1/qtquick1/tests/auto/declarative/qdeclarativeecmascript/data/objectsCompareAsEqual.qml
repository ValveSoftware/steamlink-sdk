import QtQuick 1.0

Item {
    id: root

    property variant item: child
    Item { id: child }

    property bool test1: child == child
    property bool test2: child.parent == root
    property bool test3: root != child
    property bool test4: item == child
    property bool test5: item != root
}

