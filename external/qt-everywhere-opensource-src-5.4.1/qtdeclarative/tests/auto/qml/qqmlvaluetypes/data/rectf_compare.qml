import Test 1.0

MyTypeObject {
    property real r_x: rectf.x
    property real r_y: rectf.y
    property real r_width: rectf.width
    property real r_height: rectf.height
    property variant copy: rectf
    property string tostring: rectf.toString()

    // compare to string
    property bool equalsString: (rectf == tostring)

    // compare rectfs to various value types
    property bool equalsColor: (rectf == Qt.rgba(0.2, 0.88, 0.6, 0.34))   // false
    property bool equalsVector3d: (rectf == Qt.vector3d(1, 2, 3))         // false
    property bool equalsSize: (rectf == Qt.size(1912, 1913))              // false
    property bool equalsPoint: (rectf == Qt.point(10, 4))                 // false
    property bool equalsRect: (rectf == Qt.rect(103.8, 99.2, 88.1, 77.6)) // true

    property bool equalsSelf: (rectf == rectf)                            // true
    property bool equalsOther: (rectf == Qt.rect(13.8, 9.2, 78.7, 96.2))  // false
    property bool rectfEqualsRect: (rectfrect == rect)                    // true
}

