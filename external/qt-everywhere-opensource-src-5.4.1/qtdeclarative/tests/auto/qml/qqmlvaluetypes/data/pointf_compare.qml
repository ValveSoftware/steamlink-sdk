import Test 1.0

MyTypeObject {
    property int p_x: point.x
    property int p_y: point.y
    property variant copy: point
    property string tostring: pointf.toString()

    // compare to string
    property bool equalsString: (pointf == tostring)

    // compare pointfs to various value types
    property bool equalsColor: (pointf == Qt.rgba(0.2, 0.88, 0.6, 0.34)) // false
    property bool equalsVector3d: (pointf == Qt.vector3d(1, 2, 3))       // false
    property bool equalsSize: (pointf == Qt.size(1912, 1913))            // false
    property bool equalsPoint: (pointf == Qt.point(11.3, -10.9))         // true
    property bool equalsRect: (pointf == Qt.rect(2, 3, 109, 102))        // false

    property bool equalsSelf: (pointf == pointf)               // true
    property bool equalsOther: (pointf == Qt.point(6.3, -4.9)) // false
    property bool pointfEqualsPoint: (pointfpoint == point)    // true
}
