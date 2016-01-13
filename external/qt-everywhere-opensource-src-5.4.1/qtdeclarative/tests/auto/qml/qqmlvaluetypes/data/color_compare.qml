import Test 1.0

MyTypeObject {
    property real v_r: color.r
    property real v_g: color.g
    property real v_b: color.b
    property real v_a: color.a
    property variant copy: color
    property string colorToString: color.toString()

    // compare different colors
    property bool colorEqualsIdenticalRgba: (color == Qt.rgba(0.2, 0.88, 0.6, 0.34))  // true
    property bool colorEqualsDifferentAlpha: (color == Qt.rgba(0.2, 0.88, 0.6, 0.44)) // false
    property bool colorEqualsDifferentRgba: (color == Qt.rgba(0.3, 0.98, 0.7, 0.44))  // false

    // compare different color.toString()s
    property bool colorToStringEqualsColorString: (color.toString() == colorToString)                                     // true
    property bool colorToStringEqualsDifferentAlphaString: (color.toString() == Qt.rgba(0.2, 0.88, 0.6, 0.44).toString()) // true
    property bool colorToStringEqualsDifferentRgbaString: (color.toString() == Qt.rgba(0.3, 0.98, 0.7, 0.44).toString())  // false

    // compare colors to strings
    property bool colorEqualsColorString: (color == colorToString)                                     // false
    property bool colorEqualsDifferentAlphaString: (color == Qt.rgba(0.2, 0.88, 0.6, 0.44).toString()) // false
    property bool colorEqualsDifferentRgbaString: (color == Qt.rgba(0.3, 0.98, 0.7, 0.44).toString())  // false

    // compare colors to various value types
    property bool equalsColor: (color == Qt.rgba(0.2, 0.88, 0.6, 0.34)) // true
    property bool equalsVector3d: (color == Qt.vector3d(1, 2, 3))       // false
    property bool equalsSize: (color == Qt.size(1912, 1913))            // false
    property bool equalsPoint: (color == Qt.point(10, 4))               // false
    property bool equalsRect: (color == Qt.rect(2, 3, 109, 102))        // false

    // ensure comparison directionality doesn't matter
    property bool equalsColorRHS: (Qt.rgba(0.2, 0.88, 0.6, 0.34) == color) // true
    property bool colorEqualsCopy: (color == copy) // true
    property bool copyEqualsColor: (copy == color) // true
}
