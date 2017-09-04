import Test 1.0

MyTypeObject {
    property real v_x: vector4.x
    property real v_y: vector4.y
    property real v_z: vector4.z
    property real v_w: vector4.w
    property variant copy: vector4
    property string tostring: vector4.toString()

    // compare to string
    property bool equalsString: (vector4 == tostring)

    // compare vector4ds to various value types
    property bool equalsColor: (vector4 == Qt.rgba(0.2, 0.88, 0.6, 0.34)) // false
    property bool equalsVector3d: (vector4 == Qt.vector3d(1, 2, 3))       // false
    property bool equalsSize: (vector4 == Qt.size(1912, 1913))            // false
    property bool equalsPoint: (vector4 == Qt.point(10, 4))               // false
    property bool equalsRect: (vector4 == Qt.rect(2, 3, 109, 102))        // false

    property bool equalsSelf: (vector4 == vector4) // true
}

