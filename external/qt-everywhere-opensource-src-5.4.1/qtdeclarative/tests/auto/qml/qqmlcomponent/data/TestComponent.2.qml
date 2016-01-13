import QtQuick 2.0

Item {
    id: root
    objectName: "root"
    property int zero: 0

    Item {
        id: c1
        objectName: "c1"
        property int one: zero + 1

        Item {
            id: c1c1
            objectName: "c1c1"
            property bool two: c2c1c1.two
        }

        Item {
            id: c1c2
            objectName: "c1c2"
            property string three: "three"

            Rectangle {
                id: c1c2c3
                objectName: "c1c2c3"
                property alias othercolor: c2c1.color
                color: if (c2c1.color == Qt.rgba(0,0,1)) Qt.rgba(1,0,0); else Qt.rgba(0,1,0);
            }
        }
    }

    Item {
        id: c2
        objectName: "c2"
        property string two: "two"

        Rectangle {
            id: c2c1
            objectName: "c2c1"
            property string three: "2" + c1c2.three
            color: "blue"

            MouseArea {
                id: c2c1c1
                objectName: "c2c1c1"
                property bool two: false
                onClicked: two = !two
            }

            Item {
                id: c2c1c2
                objectName: "c2c1c2"
                property string three: "1" + parent.three
            }
        }
    }

    Item {
        id: c3
        objectName: "c3"
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
        Item {}
    }

    property bool success: true
    Component.onCompleted: {
        // test state after initial bindings evaluation
        if (zero != 0) success = false;
        if (c1.one != 1) success = false;
        if (c1c1.two != false) success = false;
        if (c1c2.three != "three") success = false;
        if (c1c2c3.color != Qt.rgba(1,0,0)) success = false;
        if (c2.two != "two") success = false;
        if (c2c1.three != "2three") success = false;
        if (c2c1.color != Qt.rgba(0,0,1)) success = false;
        if (c2c1c1.two != false) success = false;
        if (c2c1c2.three != "12three") success = false;
        if (c3.children.length != 500) success = false;

        // now retrigger bindings evaluation
        root.zero = 5;
        if (c1.one != 6) success = false;
        c2c1c1.two = true;
        if (c1c1.two != true) success = false;
        c1c2.three = "3";
        if (c2c1.three != "23") success = false;
        if (c2c1c2.three != "123") success = false;
        c2c1.color = Qt.rgba(1,0,0);
        if (c1c2c3.color != Qt.rgba(0,1,0)) success = false;
        if (c1c2c3.othercolor != Qt.rgba(1,0,0)) success = false;
    }
}
