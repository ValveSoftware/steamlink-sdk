import QtQuick 1.0

QtObject {
    function testFunction() { return 19; }

    property int test: testFunction()
}
