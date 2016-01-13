import QtQuick 2.0

Item {
    property bool test: false
    property var f
    property int a: 100
    property int b

    function testFunction() {
        return a * 3;
    }

    Component.onCompleted: {
        b = Qt.binding(testFunction);
        f = Qt.binding(function() { return b + 12; });
        if (f != 312) return;
        a = 120;
        if (f != 372) return;
        test = true;
    }
}
