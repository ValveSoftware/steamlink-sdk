import QtQuick 2.0

Item {
    property bool success: false

    property variant v1: Qt.vector3d(1,2,3)
    property variant v2: Qt.vector3d(5,6,7)
    property variant m1: Qt.matrix4x4(5,5,5,5,6,6,6,6,7,7,7,7,8,8,8,8)
    property real factor: 2.23

    Component.onCompleted: {
        success = true;
        if (v1.times(v2) != Qt.vector3d(5, 12, 21)) success = false;
        if (v1.times(m1) != Qt.vector3d(1, 1, 1)) success = false;
        if (v1.times(factor) != Qt.vector3d(2.23, 4.46, 6.69)) success = false;
        if (v1.plus(v2) != Qt.vector3d(6, 8, 10)) success = false;
        if (v2.minus(v1) != Qt.vector3d(4, 4, 4)) success = false;
        if (!v1.normalized().fuzzyEquals(Qt.vector3d(0.267261, 0.534522, 0.801784), 0.00001)) success = false;
        if ((v1.length() == v2.length()) || (v1.length() != Qt.vector3d(3,2,1).length())) success = false;
        if (v1.toVector2d() != Qt.vector2d(1,2)) success = false;
        if (v1.toVector4d() != Qt.vector4d(1,2,3,0)) success = false;
        if (v1.fuzzyEquals(v2)) success = false;
        if (!v1.fuzzyEquals(v2, 4)) success = false;
    }
}
