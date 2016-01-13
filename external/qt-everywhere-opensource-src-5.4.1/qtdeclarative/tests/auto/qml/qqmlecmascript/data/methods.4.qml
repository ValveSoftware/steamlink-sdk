import QtQuick 2.0

MethodsObject {
    function testFunction2() { return 17; }
    function testFunction3() { return 16; }

    property int test: testFunction()
    property int test2: testFunction2()
    property int test3: testFunction3()
}

