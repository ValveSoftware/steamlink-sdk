import QtQuick 1.0
import Qt.test 1.0

MyRevisionedSubclass
{
    // Will not hit optimizer
    property real p1: prop1 % 3
    property real p2: prop2 % 3
    property real p3: prop3 % 3
    property real p4: prop4 % 3

    // Should hit optimizer
    property real p5: prop1
    property real p6: prop2
    property real p7: prop3
    property real p8: prop4

    Component.onCompleted: {
        method1()
        method2()
        method3()
        method4()
    }
}
