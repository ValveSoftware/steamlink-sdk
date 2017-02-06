import QtQuick 2.0
import Qt.test 1.0

Item {
    property bool test: false
    property var f: b + 12
    property int a: 100
    property int b: testFunction()

    function testFunction() {
        return a * 3;
    }

    Component.onCompleted: {
        if (f != 312) return;
        a = 120;
        if (f != 372) return;
        test = true;
    }
}
