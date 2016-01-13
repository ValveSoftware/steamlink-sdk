import QtQuick 2.0

Rectangle {
    width: 360
    height: 360

    function circularObject() {
        var a = {}
        var b = {}

        a.test = 100;
        a.c = b;
        b.c = a;
        return a;
    }
}
