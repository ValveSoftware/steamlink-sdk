import QtQuick 2.0

Item {
    property bool success: false

    property variant v1: Qt.vector2d(1,2)
    property variant v2: Qt.vector2d(5,6)
    property real factor: 2.23

    Component.onCompleted: {
        success = true;
        if (v1.times(v2) != Qt.vector2d(5, 12)) success = false;
        if (v1.times(factor) != Qt.vector2d(2.23, 4.46)) success = false;
        if (v1.plus(v2) != Qt.vector2d(6, 8)) success = false;
        if (v2.minus(v1) != Qt.vector2d(4, 4)) success = false;
        if (!v1.normalized().fuzzyEquals(Qt.vector2d(0.447214, 0.894427), 0.000001)) success = false;
        if ((v1.length() == v2.length()) || (v1.length() != Qt.vector2d(2,1).length())) success = false;
        if (v1.toVector3d() != Qt.vector3d(1,2,0)) success = false;
        if (v1.toVector4d() != Qt.vector4d(1,2,0,0)) success = false;
        if (v1.fuzzyEquals(v2)) success = false;
        if (!v1.fuzzyEquals(v2, 4)) success = false;
    }
}
