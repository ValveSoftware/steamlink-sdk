import Test 1.0

MyTypeObject {
    property int p_x: point.x
    property int p_y: point.y
    property variant copy: point
    property string tostring: point.toString()

    // compare to string
    property bool equalsString: (point == tostring)

    // compare points to various value types
    property bool equalsColor: (point == Qt.rgba(0.2, 0.88, 0.6, 0.34)) // false
    property bool equalsVector3d: (point == Qt.vector3d(1, 2, 3))       // false
    property bool equalsSize: (point == Qt.size(1912, 1913))            // false
    property bool equalsPoint: (point == Qt.point(10, 4))               // true
    property bool equalsRect: (point == Qt.rect(2, 3, 109, 102))        // false

    property bool equalsSelf: (point == point)              // true
    property bool equalsOther: (point == Qt.point(15, 4))   // false
    property bool pointEqualsPointf: (point == pointfpoint) // true
}
