import Test 1.0

MyTypeObject {
    property int r_x: rect.x
    property int r_y: rect.y
    property int r_width: rect.width
    property int r_height: rect.height
    property variant copy: rect
    property string tostring: rect.toString()

    // compare to string
    property bool equalsString: (rect == tostring)

    // compare rects to various value types
    property bool equalsColor: (rect == Qt.rgba(0.2, 0.88, 0.6, 0.34)) // false
    property bool equalsVector3d: (rect == Qt.vector3d(1, 2, 3))       // false
    property bool equalsSize: (rect == Qt.size(1912, 1913))            // false
    property bool equalsPoint: (rect == Qt.point(10, 4))               // false
    property bool equalsRect: (rect == Qt.rect(2, 3, 109, 102))        // true

    property bool equalsSelf: (rect == rect)                   // true
    property bool equalsOther: (rect == Qt.rect(6, 9, 99, 92)) // false
    property bool rectEqualsRectf: (rect == rectfrect)         // true
}

