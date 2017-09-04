import QtQuick 2.0
import Test 1.0

MyTypeObject {
    property bool success: false

    // the values come from the MyTypeObject property definitions,
    // which were defined in C++.

    property rect g: rectf
    property point p: pointf
    property size z: sizef

    property vector2d v2: vector2
    property vector3d v3: vector
    property vector4d v4: vector4
    property quaternion q: quaternion
    property matrix4x4 m: matrix
    property color c: color
    property font f: font

    Component.onCompleted: {
        success = true;

        // ensure that the semantics of the properties
        // defined in C++ match those of the properties
        // defined in QML, and that we can compare/assign etc.

        if (g != rectf) success = false;
        g = Qt.rect(1,2,3,4);
        if (g == rectf) success = false;
        g = rectf;
        if (g != rectf) success = false;
        g = rect;
        if (g != rect) success = false;
        g = rectf; // for the cpp-size value comparison.

        if (p != pointf) success = false;
        p = Qt.point(1,2);
        if (p == pointf) success = false;
        p = pointf;
        if (p != pointf) success = false;
        p = point;
        if (p != point) success = false;
        p = pointf; // for the cpp-size value comparison.

        if (z != sizef) success = false;
        z = Qt.size(1,2);
        if (z == sizef) success = false;
        z = sizef;
        if (z != sizef) success = false;
        z = size;
        if (z != size) success = false;
        z = sizef; // for the cpp-size value comparison.

        if (v2 != vector2) success = false;
        v2 = Qt.vector2d(1,2);
        if (v2 == vector2) success = false;
        v2 = vector2;
        if (v2 != vector2) success = false;

        if (v3 != vector) success = false;
        v3 = Qt.vector3d(1,2,3);
        if (v3 == vector) success = false;
        v3 = vector;
        if (v3 != vector) success = false;

        if (v4 != vector4) success = false;
        v4 = Qt.vector4d(1,2,3,4);
        if (v4 == vector4) success = false;
        v4 = vector4;
        if (v4 != vector4) success = false;

        if (q != quaternion) success = false;
        q = Qt.quaternion(1,2,3,4);
        if (q == quaternion) success = false;
        q = quaternion;
        if (q != quaternion) success = false;

        if (m != matrix) success = false;
        m = Qt.matrix4x4(120, 230, 340, 450, 560, 670, 780, 890, 900, 1010, 1120, 1230, 1340, 1450, 1560, 1670);
        if (m == matrix) success = false;
        m = matrix;
        if (m != matrix) success = false;

        if (c != color) success = false;
        c = Qt.rgba(1,0,0,.5);
        if (c == color) success = false;
        c = color;
        if (c != color) success = false;

        if (f != font) success = false;
        f = Qt.font({family: "Arial", pointSize: 15, weight: Font.DemiBold, italic: false});
        if (f == font) success = false;
        f = font;
        if (f != font) success = false;
    }
}
