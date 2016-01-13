import QtQml 2.0

QtObject {
    property bool qtqmlTypeSuccess: false
    property bool qtquickTypeSuccess: false

    property int i: 10
    property bool b: true
    property real r: 5.5
    property string s: "Hello"

    property date d: new Date(1999, 8, 8)

    property rect g: Qt.rect(1, 2, 3, 4)
    property point p: Qt.point(1, 2)
    property size z: Qt.size(1, 2)

    // the following property types are valid syntax in QML
    // but their valuetype implementation is provided by QtQuick.
    // Thus, we can define properties of the type, but not use them.
    property vector2d v2
    property vector3d v3
    property vector4d v4
    property quaternion q
    property matrix4x4 m
    property color c
    property font f

    Component.onCompleted: {
        qtqmlTypeSuccess = true;
        qtquickTypeSuccess = true;

        // test that the base qtqml provided types work
        if (i != 10) qtqmlTypeSuccess = false;
        if (b != true) qtqmlTypeSuccess = false;
        if (r != 5.5) qtqmlTypeSuccess = false;
        if (s != "Hello") qtqmlTypeSuccess = false;
        if (d.toDateString() != (new Date(1999,8,8)).toDateString()) qtqmlTypeSuccess = false;
        if (g != Qt.rect(1, 2, 3, 4)) qtqmlTypeSuccess = false;
        if (p != Qt.point(1, 2)) qtqmlTypeSuccess = false;
        if (z != Qt.size(1, 2)) qtqmlTypeSuccess = false;

        // This should also work, as the base value types are provided by QtQml.
        if (g.x != 1 || g.y != 2 || g.width != 3 || g.height != 4) qtqmlTypeSuccess = false;
        if (p.x != 1 || p.y != 2) qtqmlTypeSuccess = false;
        if (z.width != 1 || z.height != 2) qtqmlTypeSuccess = false;
    }
}
