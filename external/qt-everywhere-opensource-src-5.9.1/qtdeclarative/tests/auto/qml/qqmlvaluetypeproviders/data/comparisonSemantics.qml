import QtQuick 2.0

QtObject {
    property bool comparisonSuccess: false

    property date d: new Date(1999, 8, 8)
    property date d2: new Date(1998, 8, 8)

    property rect g: Qt.rect(1, 2, 3, 4)
    property rect g2: Qt.rect(5, 6, 7, 8)
    property point p: Qt.point(1, 2)
    property point p2: Qt.point(3, 4)
    property size z: Qt.size(1, 2)
    property size z2: Qt.size(3, 4)

    property vector2d v2: Qt.vector2d(1,2)
    property vector2d v22: Qt.vector2d(3,4)
    property vector3d v3: Qt.vector3d(1,2,3)
    property vector3d v32: Qt.vector3d(4,5,6)
    property vector4d v4: Qt.vector4d(1,2,3,4)
    property vector4d v42: Qt.vector4d(5,6,7,8)
    property quaternion q: Qt.quaternion(1,2,3,4)
    property quaternion q2: Qt.quaternion(5,6,7,8)
    property matrix4x4 m: Qt.matrix4x4(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16)
    property matrix4x4 m2: Qt.matrix4x4(21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36)
    property color c: "red"
    property color c2: "blue"
    property font f: Qt.font({family: "Arial", pointSize: 20})
    property font f2: Qt.font({family: "Arial", pointSize: 22})

    Component.onCompleted: {
        comparisonSuccess = true;

        // same type comparison.
        if (d == d2) comparisonSuccess = false;
        d = d2;
        if (d == d2) comparisonSuccess = false; // QML date uses same comparison semantics as JS date!
        if (d.toString() != d2.toString()) comparisonSuccess = false;

        if (g == g2) comparisonSuccess = false;
        g = g2;
        if (g != g2) comparisonSuccess = false;

        if (p == p2) comparisonSuccess = false;
        p = p2;
        if (p != p2) comparisonSuccess = false;

        if (z == z2) comparisonSuccess = false;
        z = z2;
        if (z != z2) comparisonSuccess = false;

        if (v2 == v22) comparisonSuccess = false;
        v2 = v22;
        if (v2 != v22) comparisonSuccess = false;

        if (v3 == v32) comparisonSuccess = false;
        v3 = v32;
        if (v3 != v32) comparisonSuccess = false;

        if (v4 == v42) comparisonSuccess = false;
        v4 = v42;
        if (v4 != v42) comparisonSuccess = false;

        if (q == q2) comparisonSuccess = false;
        q = q2;
        if (q != q2) comparisonSuccess = false;

        if (m == m2) comparisonSuccess = false;
        m = m2;
        if (m != m2) comparisonSuccess = false;

        if (c == c2) comparisonSuccess = false;
        c = c2;
        if (c != c2) comparisonSuccess = false;

        if (f == f2) comparisonSuccess = false;
        f = f2;
        if (f != f2) comparisonSuccess = false;

        // cross-type comparison.
        p = Qt.point(1,2);
        z = Qt.size(1,2);
        v2 = Qt.vector2d(1,2);
        if (p == z || p == v2 || z == v2) comparisonSuccess = false;
        if (z == p || v2 == p || v2 == z) comparisonSuccess = false;

        g = Qt.rect(1,2,3,4);
        q = Qt.quaternion(1,2,3,4);
        v4 = Qt.vector4d(1,2,3,4);
        if (g == q || g == v4 || q == v4) comparisonSuccess = false;
        if (q == g || v4 == g || v4 == q) comparisonSuccess = false;

        if (c == f) comparisonSuccess = false;
    }
}
