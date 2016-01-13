import QtQuick 2.0

Grid {
    columns: 2
    width: 100; height: 100
    function verify() {
        if (item1.KeyNavigation.right != item2)
            return false;
        if (item1.KeyNavigation.down != item3)
            return false;
        if (item1.KeyNavigation.tab != item2)
            return false;
        if (item1.KeyNavigation.backtab != item4)
            return false;

        if (item2.KeyNavigation.left != item1)
            return false;
        if (item2.KeyNavigation.down != item4)
            return false;
        if (item2.KeyNavigation.tab != item3)
            return false;
        if (item2.KeyNavigation.backtab != item1)
            return false;

        if (item3.KeyNavigation.right != item4)
            return false;
        if (item3.KeyNavigation.up != item1)
            return false;
        if (item3.KeyNavigation.tab != item4)
            return false;
        if (item3.KeyNavigation.backtab != item2)
            return false;

        if (item4.KeyNavigation.left != item3)
            return false;
        if (item4.KeyNavigation.up != item2)
            return false;
        if (item4.KeyNavigation.tab != item1)
            return false;
        if (item4.KeyNavigation.backtab != item3)
            return false;

        return true;
    }

    Rectangle {
        id: item1
        objectName: "item1"
        focus: true
        width: 50; height: 50
        color: focus ? "red" : "lightgray"
        KeyNavigation.right: item2
        KeyNavigation.down: item3
        KeyNavigation.tab: item2
        KeyNavigation.backtab: item4
    }
    Rectangle {
        id: item2
        objectName: "item2"
        width: 50; height: 50
        color: focus ? "red" : "lightgray"
        KeyNavigation.left: item1
        KeyNavigation.down: item4
        KeyNavigation.tab: item3
        KeyNavigation.backtab: item1
    }
    Rectangle {
        id: item3
        objectName: "item3"
        width: 50; height: 50
        color: focus ? "red" : "lightgray"
        KeyNavigation.right: item4
        KeyNavigation.up: item1
        KeyNavigation.tab: item4
        KeyNavigation.backtab: item2
    }
    Rectangle {
        id: item4
        objectName: "item4"
        width: 50; height: 50
        color: focus ? "red" : "lightgray"
        KeyNavigation.left: item3
        KeyNavigation.up: item2
        KeyNavigation.tab: item1
        KeyNavigation.backtab: item3
    }
}
