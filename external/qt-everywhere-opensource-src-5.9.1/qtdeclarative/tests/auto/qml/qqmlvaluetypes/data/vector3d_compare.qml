import Test 1.0

MyTypeObject {
    property real v_x: vector.x
    property real v_y: vector.y
    property real v_z: vector.z
    property variant copy: vector
    property string tostring: vector.toString()

    // compare to string
    property bool equalsString: (vector == tostring)

    // compare vector3ds to various value types
    property bool equalsColor: (vector == Qt.rgba(0.2, 0.88, 0.6, 0.34))   // false
    property bool equalsVector3d: (vector == Qt.vector3d(23.88, 3.1, 4.3)) // true
    property bool equalsSize: (vector == Qt.size(1912, 1913))              // false
    property bool equalsPoint: (vector == Qt.point(10, 4))                 // false
    property bool equalsRect: (vector == Qt.rect(2, 3, 109, 102))          // false

    property bool equalsSelf: (vector == vector)                        // true
    property bool equalsOther: (vector == Qt.vector3d(3.1, 2.2, 923.2)) // false
}

