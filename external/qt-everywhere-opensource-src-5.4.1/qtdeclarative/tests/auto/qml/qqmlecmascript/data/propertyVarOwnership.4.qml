import QtQuick 2.0

Item {
    id: root

    property var object

    property bool test: false

    Component.onCompleted: {
        var c = Qt.createComponent("propertyVarOwnership.4.type1.qml");
        object = c.createObject();

        if (object.dummy != 10) return;
        if (object.test != true) return;

        object.creatorRef = root;

        test = true;
    }

    function runTest() {
        object = null;
    }
}
