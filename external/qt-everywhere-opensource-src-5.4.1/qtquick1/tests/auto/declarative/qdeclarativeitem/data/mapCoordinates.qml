import QtQuick 1.0

Item {
    id: root; objectName: "root"
    width: 200; height: 200

    Item { id: itemA; objectName: "itemA"; x: 50; y: 50 }

    Item {
        x: 50; y: 50
        Item { id: itemB; objectName: "itemB"; x: 100; y: 100 }
    }

    function mapAToB(x, y) {
        var pos = itemA.mapToItem(itemB, x, y)
        return Qt.point(pos.x, pos.y)
    }

    function mapAFromB(x, y) {
        var pos = itemA.mapFromItem(itemB, x, y)
        return Qt.point(pos.x, pos.y)
    }

    function mapAToNull(x, y) {
        var pos = itemA.mapToItem(null, x, y)
        return Qt.point(pos.x, pos.y)
    }

    function mapAFromNull(x, y) {
        var pos = itemA.mapFromItem(null, x, y)
        return Qt.point(pos.x, pos.y)
    }

    function checkMapAToInvalid(x, y) {
        var pos = itemA.mapToItem(1122, x, y)
        return pos.x == undefined && pos.y == undefined
    }

    function checkMapAFromInvalid(x, y) {
        var pos = itemA.mapFromItem(1122, x, y)
        return pos.x == undefined && pos.y == undefined
    }
}
