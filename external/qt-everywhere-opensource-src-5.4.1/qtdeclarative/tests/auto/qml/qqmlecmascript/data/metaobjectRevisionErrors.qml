import QtQuick 2.0
import Qt.test 1.0

MyRevisionedClass
{
    // Will not hit optimizer
    property real p1: prop1 % 3
    property real p2: prop2 % 3

    // Should hit optimizer
    property real p3: prop2

    Component.onCompleted: method2()
}
