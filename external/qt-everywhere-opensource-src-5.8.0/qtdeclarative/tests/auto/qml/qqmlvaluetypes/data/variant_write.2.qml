import QtQuick 2.0

Item {
    property bool complete: false
    property bool success: false

    property variant v1

    Component.onCompleted: {
        complete = false;
        success = true;

        // store a js reference to the VariantReference vt in a temp var.
        v1 = Qt.vector3d(1,2,3)
        var ref = v1;
        ref.z = 5
        if (v1 != Qt.vector3d(1,2,5)) success = false;

        // now change the type of a reference, and attempt a valid write.
        v1 = Qt.vector2d(1,2);
        ref.x = 5;
        if (v1 != Qt.vector2d(5,2)) success = false;

        // now change the type of a reference, and attempt an invalid write.
        v1 = Qt.rgba(1,0,0,1);
        ref.x = 5;
        if (v1.toString() != Qt.rgba(1,0,0,1).toString()) success = false;
        v1 = 6;
        ref.x = 5;
        if (v1 != 6) success = false;

        // now change the type of a reference, and attempt an invalid read.
        v1 = Qt.vector3d(1,2,3);
        ref = v1;
        v1 = Qt.vector2d(1,2);
        if (ref.z == 3) success = false;

        complete = true;
    }
}
