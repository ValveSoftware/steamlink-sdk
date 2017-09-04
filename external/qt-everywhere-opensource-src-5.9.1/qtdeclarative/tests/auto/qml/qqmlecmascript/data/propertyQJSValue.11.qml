import QtQuick 2.0
import Qt.test 1.0

Item {
    property bool test: false

    MyQmlObject { id: fnResultObj; qjsvalue: testFunction(5) }
    MyQmlObject { id: f1Obj; qjsvalue: testFunction }
    MyQmlObject { id: f2Obj; qjsvalue: testFunction }


    property alias fnResult: fnResultObj.qjsvalue;
    property alias f1: f1Obj.qjsvalue
    property alias f2: f2Obj.qjsvalue

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
