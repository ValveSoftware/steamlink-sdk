import Test 1.0

MyTypeObject {
    property real v_m11: matrix.m11
    property real v_m12: matrix.m12
    property real v_m13: matrix.m13
    property real v_m14: matrix.m14
    property real v_m21: matrix.m21
    property real v_m22: matrix.m22
    property real v_m23: matrix.m23
    property real v_m24: matrix.m24
    property real v_m31: matrix.m31
    property real v_m32: matrix.m32
    property real v_m33: matrix.m33
    property real v_m34: matrix.m34
    property real v_m41: matrix.m41
    property real v_m42: matrix.m42
    property real v_m43: matrix.m43
    property real v_m44: matrix.m44
    property variant copy: matrix
    property string tostring: matrix.toString()

    // compare to string
    property bool equalsString: (matrix == tostring)

    // compare matrix4x4s to various value types
    property bool equalsColor: (matrix == Qt.rgba(0.2, 0.88, 0.6, 0.34)) // false
    property bool equalsVector3d: (matrix == Qt.vector3d(1, 2, 3))       // false
    property bool equalsSize: (matrix == Qt.size(1912, 1913))            // false
    property bool equalsPoint: (matrix == Qt.point(10, 4))               // false
    property bool equalsRect: (matrix == Qt.rect(2, 3, 109, 102))        // false

    property bool equalsSelf: (matrix == matrix) // true
}

