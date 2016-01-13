import QtQuick 2.0

QtObject {
    property int x;
    property int y;
    property int z;

    Component.onCompleted: {
        var vec3 = Qt.vector3d(1, 2, 3);
        x = vec3.x;
        y = vec3.y;
        z = vec3.z;
    }
}
