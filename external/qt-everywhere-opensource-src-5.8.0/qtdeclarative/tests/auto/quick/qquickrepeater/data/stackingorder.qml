import QtQuick 2.0

Item {
    id: root
    property bool stackingOrderOk: true

    function verifyStackingOrder() {
        var len = children.length
        for (var i = 0; i < len; ++i) {
            var expectedName;
            if (i === 0) {
                expectedName = "first"
            } else if (i === len - 2) {
                expectedName = "repeater"
            } else if (i === len - 1) {
                expectedName = "last"
            } else {
                expectedName = "middle" + (i - 1)
            }
            if (children[i].objectName !== expectedName)
                stackingOrderOk = false
        }
    }
    Item {
        objectName: "first"
    }
    Repeater {
        objectName: "repeater"
        model: 1
        Item {
            objectName: "middle" + index
            Component.onCompleted: { verifyStackingOrder();}
        }
    }
    Item {
        objectName: "last"
    }
}
