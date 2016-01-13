import QtQuick 2.0

Item {
    property bool complete: false
    property bool success: false

    property variant v1: Qt.vector3d(1,2,3)
    property variant v2: Qt.vector3d(4,5,6)
    property variant v3: v1.plus(v2) // set up doomed binding

    Component.onCompleted: {
        complete = false;
        success = true;

        v1 = Qt.vector2d(1,2) // changing the type of the property shouldn't cause crash

        v1.x = 8;
        v2.x = 9;

        if (v1 != Qt.vector2d(8,2)) success = false;
        if (v2 != Qt.vector3d(9,5,6)) success = false;

        complete = true;
    }
}
