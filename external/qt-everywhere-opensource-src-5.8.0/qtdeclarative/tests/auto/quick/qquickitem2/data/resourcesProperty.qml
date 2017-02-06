import QtQuick 2.0

Item {
    id: root

    property bool test1
    property bool test2
    property bool test3
    property bool test4
    property bool test5
    property bool test6

    Component.onCompleted: {
        test1 = (root.resources.length === 4)
        test2 = root.resources[0] === item1
        test3 = root.resources[1] === item2
        test4 = root.resources[2] === item3
        test5 = root.resources[3] === otherObject
        test6 = root.resources[10] == null
    }

    //Resources can be used explicitly
    resources: [ Item { id: item1 }, Item { id: item2 }, Item { id: item3 } ]

    Item {
    //Item in Data go the children property.
    }

    QtObject {
        //Objects in Data which are not items are put in resources.
        id: otherObject
        objectName: "subObject";
    }
}
