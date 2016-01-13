import QtQuick 2.0

Item {
    property int a: 0
    property int b: 14

    function b() { return 11; }
    function c() { return 33; }

    QtObject {
        id: a
        property int value: 19
    }

    QtObject {
        id: c
        property int value: 24
    }

    QtObject {
        id: nested
        property int a: 1
        property int test: a.value
        property int test2: b
        property int test3: c.value
    }


    // id takes precedence over local, and root properties
    property int test1: a.value
    property alias test2: nested.test

    // properties takes precedence over local, and root methods
    property int test3: b
    property alias test4: nested.test2

    // id takes precedence over methods
    property int test5: c.value
    property alias test6: nested.test3
}
