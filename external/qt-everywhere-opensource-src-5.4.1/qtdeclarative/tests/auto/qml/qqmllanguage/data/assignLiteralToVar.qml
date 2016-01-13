// This tests assigning literals to "var" properties.
// These properties store JavaScript object references.

import QtQuick 2.0

QtObject {
    property var test1: 1
    property var test2: 1.7
    property var test3: "Hello world!"
    property var test4: "#FF008800"
    property var test5: "10,10,10x10"
    property var test6: "10,10"
    property var test7: "10x10"
    property var test8: "100,100,100"
    property var test9: String("#FF008800")
    property var test10: true
    property var test11: false

    property variant variantTest1Bound: test1 + 4 // 1 + 4 + 4 = 9

    property var test12: Qt.rgba(0.2, 0.3, 0.4, 0.5)
    property var test13: Qt.rect(10, 10, 10, 10)
    property var test14: Qt.point(10, 10)
    property var test15: Qt.size(10, 10)
    property var test16: Qt.vector3d(100, 100, 100)

    property var test1Bound: test1 + 6 // 1 + 4 + 6 = 11

    Component.onCompleted: {
        test1 = test1 + 4;
    }
}
