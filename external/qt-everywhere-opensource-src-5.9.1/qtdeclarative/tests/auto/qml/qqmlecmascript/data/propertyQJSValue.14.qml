
import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    property bool test: false
    property int a: 100
    property int b

    function testFunction() {
        return a * 3;
    }

    Component.onCompleted: {
        b = Qt.binding(testFunction);
        qjsvalue = Qt.binding(function() { return b + 12; });
        if (qjsvalue != 312) return;
        a = 120;
        if (qjsvalue != 372) return;
        test = true;
    }
}
