import QtQuick 2.0

Item {
    property bool test: false
    property var fnResult: testFunction(5)
    property var f1: testFunction
    property var f2

    function testFunction(x) {
        return x;
    }

    Component.onCompleted: {
        f2 = testFunction;
        if (fnResult != 5) return;
        if (f1(6) != 6) return;
        if (f2(7) != 7) return;
        if (f1 != f2) return;
        test = true;
    }
}
