import Test 1.0

MyTypeObject {
    property int s_width: size.width
    property int s_height: size.height
    property variant copy: size
    property string tostring: size.toString()

    // compare to string
    property bool equalsString: (size == tostring)

    // compare sizes to various value types
    property bool equalsColor: (size == Qt.rgba(0.2, 0.88, 0.6, 0.34)) // false
    property bool equalsVector3d: (size == Qt.vector3d(1, 2, 3))       // false
    property bool equalsSize: (size == Qt.size(1912, 1913))            // true
    property bool equalsPoint: (size == Qt.point(10, 4))               // false
    property bool equalsRect: (size == Qt.rect(2, 3, 109, 102))        // false

    property bool equalsSelf: (size == size)                 // true
    property bool equalsOther: (size == Qt.size(1212, 1313)) // false
    property bool sizeEqualsSizef: (size == sizefsize)       // true
}

