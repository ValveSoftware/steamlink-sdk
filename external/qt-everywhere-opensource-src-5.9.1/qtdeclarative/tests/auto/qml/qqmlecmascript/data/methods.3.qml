import QtQuick 2.0

QtObject {
    function testFunction() { return 19; }

    property int test: testFunction()
}
