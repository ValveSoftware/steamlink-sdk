import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    property bool test: false
    qjsvalue: [ Qt.binding(function() { return testFunction() + 12; }) ]
    property int a: 100
    property int b

    function testFunction() {
        return a * 3;
    }

    Component.onCompleted: {
        b = qjsvalue[0];
        if (b != 312) return;
        a = 120;
        if (b != 372) return;
        test = true;
    }
}
