import QtQuick 2.3

Item {
    id: root
    width: 300
    height: 300

    function childContainsViaMapToItem(x, y) {
        var childLocalPos = root.mapToItem(child, x, y);
        return child.contains(childLocalPos);
    }

    function childContainsViaMapFromItem(x, y) {
        var childLocalPos = child.mapFromItem(root, x, y);
        return child.contains(childLocalPos);
    }

    Item {
        id: child
        x: 50
        y: 50
        width: 50
        height: 50
    }
}

