import Test 1.0

MyTypeObject {
    property real s_width: sizef.width
    property real s_height: sizef.height
    property variant copy: sizef
    property string tostring: sizef.toString()

    // compare to string
    property bool equalsString: (sizef == tostring)

    // compare sizefs to various value types
    property bool equalsColor: (sizef == Qt.rgba(0.2, 0.88, 0.6, 0.34)) // false
    property bool equalsVector3d: (sizef == Qt.vector3d(1, 2, 3))       // false
    property bool equalsSize: (sizef == Qt.size(0.1, 100923.2))         // true
    property bool equalsPoint: (sizef == Qt.point(10, 4))               // false
    property bool equalsRect: (sizef == Qt.rect(2, 3, 109, 102))        // false

    property bool equalsSelf: (sizef == sizef)               // true
    property bool equalsOther: (size == Qt.size(3.1, 923.2)) // false
    property bool sizefEqualsSize: (sizefsize == size)       // true
}


