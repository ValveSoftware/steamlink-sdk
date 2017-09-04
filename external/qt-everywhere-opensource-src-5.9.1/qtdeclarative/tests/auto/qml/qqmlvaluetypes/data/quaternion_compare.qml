import Test 1.0

MyTypeObject {
    property real v_scalar: quaternion.scalar
    property real v_x: quaternion.x
    property real v_y: quaternion.y
    property real v_z: quaternion.z
    property variant copy: quaternion
    property string tostring: quaternion.toString()

    // compare to string
    property bool equalsString: (quaternion == tostring)

    // compare quaternions to various value types
    property bool equalsColor: (quaternion == Qt.rgba(0.2, 0.88, 0.6, 0.34)) // false
    property bool equalsVector3d: (quaternion == Qt.vector3d(1, 2, 3))       // false
    property bool equalsSize: (quaternion == Qt.size(1912, 1913))            // false
    property bool equalsPoint: (quaternion == Qt.point(10, 4))               // false
    property bool equalsRect: (quaternion == Qt.rect(2, 3, 109, 102))        // false

    property bool equalsSelf: (quaternion == quaternion) // true
}

