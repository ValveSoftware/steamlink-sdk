import QtQuick 2.0

Item {
    property var object

    property bool test1: false
    property bool test2: false

    // Test methods are executed in sequential order

    function runTest() {
        var c = Qt.createComponent("propertyVarOwnership.3.type.qml");
        object = c.createObject();

        if (object.dummy != 10) return;
        test1 = true;
    }

    // Run gc() from C++

    function runTest2() {
        if (object.dummy != 10) return;

        object = undefined;
        if (object != undefined) return;

        test2 = true;
    }

    // Run gc() from C++ - QObject should be collected
}
