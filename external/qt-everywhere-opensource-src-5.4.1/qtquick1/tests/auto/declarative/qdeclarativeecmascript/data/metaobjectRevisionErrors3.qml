import QtQuick 1.0
import Qt.test 1.1

MyRevisionedSubclass
{
    // Will not hit optimizer
    property real pA: propA % 3
    property real pB: propB % 3
    property real pC: propC % 3
    property real pD: propD % 3
    property real p1: prop1 % 3
    property real p2: prop2 % 3
    property real p3: prop3 % 3
    property real p4: prop4 % 3

    // Should hit optimizer
    property real pE: propA
    property real pF: propB
    property real pG: propC
    property real pH: propD
    property real p5: prop1
    property real p6: prop2
    property real p7: prop3
    property real p8: prop4

    Component.onCompleted: {
        methodA()
        methodB()
        methodC()
        methodD()
        method1()
        method2()
        method3()
        method4()
    }
}
