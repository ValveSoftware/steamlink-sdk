import QtQuick 1.0

Item {
    property alias blah: item.x
    Item { id: item }

    function testFunction() { return 9; }
    property int test: testFunction();
}
