import Test 1.0

MyTypeObject {
    property real v_x: vector2.x
    property real v_y: vector2.y
    property variant copy: vector2
    property string tostring: vector2.toString()

    // compare to string
    property bool equalsString: (vector2 == tostring)

    // compare vector2ds to various value types
    property bool equalsColor: (vector2 == Qt.rgba(0.2, 0.88, 0.6, 0.34)) // false
    property bool equalsVector3d: (vector2 == Qt.vector3d(1, 2, 3))       // false
    property bool equalsSize: (vector2 == Qt.size(1912, 1913))            // false
    property bool equalsPoint: (vector2 == Qt.point(10, 4))               // false
    property bool equalsRect: (vector2 == Qt.rect(2, 3, 109, 102))        // false

    property bool equalsSelf: (vector2 == vector2)
}

