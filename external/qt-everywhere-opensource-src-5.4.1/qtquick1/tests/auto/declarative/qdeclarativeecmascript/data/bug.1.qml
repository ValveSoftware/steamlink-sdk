import QtQuick 1.0

QtObject {
    property int a: 10
    property bool b: false

    property int test

    test: ((a == 10)?(a + 1):0) + ((b == true)?9:3)
}
