import Test 1.0

MyTypeObject {
    property string f_family: font.family
    property bool f_bold: font.bold
    property int f_weight: font.weight
    property bool f_italic: font.italic
    property bool f_underline: font.underline
    property bool f_overline: font.overline
    property bool f_strikeout: font.strikeout
    property real f_pointSize: font.pointSize
    property int f_pixelSize: font.pixelSize
    property int f_capitalization: font.capitalization
    property real f_letterSpacing: font.letterSpacing
    property real f_wordSpacing: font.wordSpacing;
    property variant copy: font
    property string tostring: font.toString()

    // compare to string
    property bool equalsString: (font == tostring)

    // compare fonts to various value types
    property bool equalsColor: (font == Qt.rgba(0.2, 0.88, 0.6, 0.34)) // false
    property bool equalsVector3d: (font == Qt.vector3d(1, 2, 3))       // false
    property bool equalsSize: (font == Qt.size(1912, 1913))            // false
    property bool equalsPoint: (font == Qt.point(10, 4))               // false
    property bool equalsRect: (font == Qt.rect(2, 3, 109, 102))        // false

    property bool equalsSelf: (font == font) // true
}

