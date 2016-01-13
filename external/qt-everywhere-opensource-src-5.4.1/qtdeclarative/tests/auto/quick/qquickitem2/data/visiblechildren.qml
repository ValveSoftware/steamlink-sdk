import QtQuick 2.0

Item {
    id: root
    width: 400
    height: 300

    Row {
        id: row
        Item { id: item1
            Item { id: item1_1; visible: true }
            Item { id: item1_2; visible: true }
        }
        Item { id: item2 }
        Item { id: item3
            Item { id: item3_1; visible: false }
            Item { id: item3_2; visible: false }
        }
        Item { id: item4; visible: false
            Item { id: item4_1          // implicitly invisible
                Item { id: item4_1_1 }  // implicitly invisible
                Item { id: item4_1_2 }  // implicitly invisible
            }
        }
    }

    property int row_changeEventCalls: 0
    property int item1_changeEventCalls: 0
    property int item2_changeEventCalls: 0
    property int item3_changeEventCalls: 0
    property int item4_1_changeEventCalls: 0
    property int item4_1_1_changeEventCalls: 0
    Connections { target: row; onVisibleChildrenChanged: row_changeEventCalls++ }
    Connections { target: item1; onVisibleChildrenChanged: item1_changeEventCalls++ }
    Connections { target: item2; onVisibleChildrenChanged: item2_changeEventCalls++ }
    Connections { target: item3; onVisibleChildrenChanged: item3_changeEventCalls++ }
    Connections { target: item4_1; onVisibleChildrenChanged: item4_1_changeEventCalls++ }
    Connections { target: item4_1_1; onVisibleChildrenChanged: item4_1_1_changeEventCalls++ }

    // Make sure there are three visible children and no signals fired yet
    property bool test1_1: row.visibleChildren.length == 3
    property bool test1_2: row.visibleChildren[0] == item1 && row.visibleChildren[1] == item2 && row.visibleChildren[2] == item3
    property bool test1_3: row_changeEventCalls == 0
    property bool test1_4: item1_changeEventCalls == 0 && item2_changeEventCalls == 0 && item3_changeEventCalls == 0

    // Next test
    function hideFirstAndLastRowChild() {
        item1.visible = false;
        item3.visible = false;
    }

    // Make sure row is signaled twice and item1 only once, and item3 not at all, and that item2 is the visible child
    property bool test2_1: row.visibleChildren.length == 1
    property bool test2_2: row.visibleChildren[0] == item2
    property bool test2_3: row_changeEventCalls == 2
    property bool test2_4: item1_changeEventCalls == 1 && item2_changeEventCalls == 0 && item3_changeEventCalls == 0

    // Next test
    function showLastRowChildsLastChild() {
        item3_2.visible = true;
    }

    // Make sure item3_changeEventCalls is not signaled
    property bool test3_1: row.visibleChildren.length == 1
    property bool test3_2: row.visibleChildren[0] == item2
    property bool test3_3: row_changeEventCalls == 2
    property bool test3_4: item1_changeEventCalls == 1 && item2_changeEventCalls == 0 && item3_changeEventCalls == 0

    // Next test
    function showLastRowChild() {
        item3.visible = true;
    }

    // Make sure row and item3 are signaled
    property bool test4_1: row.visibleChildren.length == 2
    property bool test4_2: row.visibleChildren[0] == item2 && row.visibleChildren[1] == item3
    property bool test4_3: row_changeEventCalls == 3
    property bool test4_4: item1_changeEventCalls == 1 && item2_changeEventCalls == 0 && item3_changeEventCalls == 1

    // Next test
    function tryWriteToReadonlyVisibleChildren() {
        var foo = fooComponent.createObject(root);
        if (Qt.isQtObject(foo) && foo.children.length == 3 && foo.visibleChildren.length == 3) {
            test5_1 = true;
        }

        foo.visibleChildren.length = 10;    // make sure this has no effect
        test5_1 = (foo.visibleChildren.length == 3);
        delete foo;
    }

    Component {
        id: fooComponent
        Item {
            children: [ Item {},Item {},Item {} ]
            visibleChildren: [ Item {} ]
        }
    }

    // Make sure visibleChildren.length is 3 and stays that way
    property bool test5_1: false

    // Next test
    function reparentVisibleItem3() {
        item3.parent = hiddenItem;  // item3 has one visible children
    }

    Item { id: hiddenItem; visible: false }

    property bool test6_1: row.visibleChildren.length == 1 && row_changeEventCalls == 4
    property bool test6_2: item3_changeEventCalls == 2
    property bool test6_3: item3.visible == false

    // Next test

    property bool test6_4: item4_1.visible == false && item4_1_changeEventCalls == 0

    function reparentImlicitlyInvisibleItem4_1() {
        item4_1.parent = visibleItem;
    }

    Item { id: visibleItem; visible: true }
    property int visibleItem_changeEventCalls: 0
    Connections { target: visibleItem; onVisibleChildrenChanged: visibleItem_changeEventCalls++ }


    // Make sure that an item with implictly invisible children will be signaled when reparented to a visible parent
    property bool test7_1: row.visibleChildren.length == 1 && row_changeEventCalls == 4
    property bool test7_2: item4_1.visible == true
    property bool test7_3: item4_1_changeEventCalls == 1
    property bool test7_4: visibleItem_changeEventCalls == 1



    // FINALLY make sure nothing has changes while we weren't paying attention

    property bool test8_1: row.visibleChildren.length == 1 && row.visibleChildren[0] == item2 && row_changeEventCalls == 4
    property bool test8_2: item1_changeEventCalls == 1 && item1.visible == false
    property bool test8_3: item2_changeEventCalls == 0 && item2.visible == true
    property bool test8_4: item3_changeEventCalls == 2 && item3.visible == false
    property bool test8_5: item4_1_1_changeEventCalls == 0 && item4_1_1.visible == true

}
