import QtQuick 2.0

Item {
    property bool success: false
    property bool complete: false

    property matrix4x4 m1: Qt.matrix4x4(1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4)
    property matrix4x4 m2: Qt.matrix4x4(5,5,5,5,6,6,6,6,7,7,7,7,8,8,8,8)
    property vector4d v1: Qt.vector4d(1,2,3,4)
    property vector4d v2: Qt.vector4d(5,6,7,8)
    property real scalar: 5

    Component.onCompleted: {
        // test that invokable functions of non-qml module value types work
        complete = false;
        success = true;
        var result;

        result = v1.plus(v2);
        if (result != Qt.vector4d(6, 8, 10, 12)) success = false;

        result = v1.times(scalar);
        if (result != Qt.vector4d(5, 10, 15, 20)) success = false;

        result = v1.times(v2);
        if (result != Qt.vector4d(5, 12, 21, 32)) success = false;

        // ensure that side-effects don't cause overwrite of valuetype-copy values.
        result = Qt.vector4d(1,2,3,4).times(Qt.vector4d(5,6,7,8), Qt.vector4d(9,9,9,9).toString());
        if (result != Qt.vector4d(5, 12, 21, 32)) success = false;

        result = v1.times(m2);
        if (result != Qt.vector4d(70,70,70,70)) success = false;

        result = m1.times(v2);
        if (result != Qt.vector4d(26, 52, 78, 104)) success = false;

        result = m1.times(m2);
        if (result != Qt.matrix4x4(26,26,26,26,52,52,52,52,78,78,78,78,104,104,104,104)) success = false;

        result = m1.plus(m2);
        if (result != Qt.matrix4x4(6,6,6,6,8,8,8,8,10,10,10,10,12,12,12,12)) success = false;

        result = m1.row(2);    // zero-based
        if (result != Qt.vector4d(3, 3, 3, 3)) success = false;

        result = m1.column(2); // zero-based
        if (result != Qt.vector4d(1, 2, 3, 4)) success = false;

        complete = true;
    }
}
