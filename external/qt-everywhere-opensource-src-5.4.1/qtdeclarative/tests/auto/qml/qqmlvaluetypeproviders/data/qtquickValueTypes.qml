import QtQuick 2.0

QtObject {
    property bool qtqmlTypeSuccess: false
    property bool qtquickTypeSuccess: false

    property int i: 10
    property bool b: true
    property real r: 5.5
    property string s: "Hello"

    property date d: new Date(1999, 8, 8)

    property rect g: Qt.rect(1, 2, 3, 4)
    property point p: Qt.point(1, 2)
    property size z: Qt.size(1, 2)

    property vector2d v2: Qt.vector2d(1,2)
    property vector3d v3: Qt.vector3d(1,2,3)
    property vector4d v4: Qt.vector4d(1,2,3,4)
    property quaternion q: Qt.quaternion(1,2,3,4)
    property matrix4x4 m: Qt.matrix4x4(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16)
    property color c: "red"
    property color c2: "red"
    property font f: Qt.font({ family: "Arial", pointSize: 20 })

    // ensure that group property specification works as expected.
    property font f2
    f2.family: "Arial"
    f2.pointSize: 45
    f2.italic: true
    v22.x: 5
    v22.y: 10
    property vector2d v22
    property font f3 // note: cannot specify grouped subproperties inline with property declaration :-/
    f3 {
        family: "Arial"
        pointSize: 45
        italic: true
    }

    Component.onCompleted: {
        qtqmlTypeSuccess = true;
        qtquickTypeSuccess = true;

        // check base types still work even though we imported QtQuick
        if (i != 10) qtqmlTypeSuccess = false;
        if (b != true) qtqmlTypeSuccess = false;
        if (r != 5.5) qtqmlTypeSuccess = false;
        if (s != "Hello") qtqmlTypeSuccess = false;
        if (d.toDateString() != (new Date(1999,8,8)).toDateString()) qtqmlTypeSuccess = false;

        // check language-provided value types still work.
        if (g != Qt.rect(1, 2, 3, 4)) qtqmlTypeSuccess = false;
        if (g.x != 1 || g.y != 2 || g.width != 3 || g.height != 4) qtqmlTypeSuccess = false;
        if (p != Qt.point(1, 2)) qtqmlTypeSuccess = false;
        if (p.x != 1 || p.y != 2) qtqmlTypeSuccess = false;
        if (z != Qt.size(1, 2)) qtqmlTypeSuccess = false;
        if (z.width != 1 || z.height != 2) qtqmlTypeSuccess = false;

        // Check that the value type provider for vector3d and other non-QtQml value-types is provided by QtQuick.
        if (v2.x != 1 || v2.y != 2) qtquickTypeSuccess = false;
        if (v2 != Qt.vector2d(1,2)) qtquickTypeSuccess = false;
        if (v3.x != 1 || v3.y != 2 || v3.z != 3) qtquickTypeSuccess = false;
        if (v3 != Qt.vector3d(1,2,3)) qtquickTypeSuccess = false;
        if (v4.x != 1 || v4.y != 2 || v4.z != 3 || v4.w != 4) qtquickTypeSuccess = false;
        if (v4 != Qt.vector4d(1,2,3,4)) qtquickTypeSuccess = false;
        if (q.scalar != 1 || q.x != 2 || q.y != 3 || q.z != 4) qtquickTypeSuccess = false;
        if (q != Qt.quaternion(1,2,3,4)) qtquickTypeSuccess = false;
        if (m != Qt.matrix4x4(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16)) qtquickTypeSuccess = false;
        if (c != Qt.rgba(1,0,0,1)) qtquickTypeSuccess = false;
        if (c != c2) qtquickTypeSuccess = false; // can compare two colors directly.
        if (f.family != "Arial" || f.pointSize != 20) qtquickTypeSuccess = false;
        if (f != Qt.font({ family: "Arial", pointSize: 20 })) qtquickTypeSuccess = false;
        if (f2.family != "Arial" || f2.pointSize != 45 || f2.italic != true) qtquickTypeSuccess = false;
        if (f2 != f3) qtquickTypeSuccess = false;
        if (v22.x != 5 || v22.y != 10) qtquickTypeSuccess = false;

        // font has some optional parameters.
        var defaultFont = Qt.font({ family: "Arial", pointSize: 22 }); // normal should be default weight.
        var lightFont = Qt.font({ family: "Arial", pointSize: 22, weight: Font.Light });
        var normalFont = Qt.font({ family: "Arial", pointSize: 22, weight: Font.Normal });
        var demiboldFont = Qt.font({ family: "Arial", pointSize: 22, weight: Font.DemiBold });
        var boldFont = Qt.font({ family: "Arial", pointSize: 22, weight: Font.Bold });
        var blackFont = Qt.font({ family: "Arial", pointSize: 22, weight: Font.Black });

        f = Qt.font({ family: "Arial", pointSize: 22, weight: Font.Light });
        if (f.family != "Arial" || f.pointSize != 22 || f.weight != lightFont.weight || f.weight == normalFont.weight) qtquickTypeSuccess = false;
        f = Qt.font({ family: "Arial", pointSize: 22, weight: Font.Normal, italic: true });
        if (f.family != "Arial" || f.pointSize != 22 || f.weight != normalFont.weight || f.italic != true) qtquickTypeSuccess = false;
        f = Qt.font({ family: "Arial", pointSize: 22, weight: Font.DemiBold, italic: false });
        if (f.family != "Arial" || f.pointSize != 22 || f.weight != demiboldFont.weight || f.italic != false) qtquickTypeSuccess = false;
        f = Qt.font({ family: "Arial", pointSize: 22, weight: Font.Bold }); // italic should be false by default
        if (f.family != "Arial" || f.pointSize != 22 || f.weight != boldFont.weight || f.italic != false) qtquickTypeSuccess = false;
        f = Qt.font({ family: "Arial", pointSize: 22, weight: Font.Black }); // italic should be false by default
        if (f.family != "Arial" || f.pointSize != 22 || f.weight != blackFont.weight || f.italic != false) qtquickTypeSuccess = false;

        // Check the string conversion codepaths.
        v2 = "5,6";
        if (v2 != Qt.vector2d(5,6)) qtquickTypeSuccess = false;
        if (v2.toString() != "QVector2D(5, 6)") qtquickTypeSuccess = false;
        v3 = "5,6,7";
        if (v3 != Qt.vector3d(5,6,7)) qtquickTypeSuccess = false;
        if (v3.toString() != "QVector3D(5, 6, 7)") qtquickTypeSuccess = false;
        v4 = "5,6,7,8";
        if (v4 != Qt.vector4d(5,6,7,8)) qtquickTypeSuccess = false;
        if (v4.toString() != "QVector4D(5, 6, 7, 8)") qtquickTypeSuccess = false;
        q = "5,6,7,8";
        if (q != Qt.quaternion(5,6,7,8)) qtquickTypeSuccess = false;
        if (q.toString() != "QQuaternion(5, 6, 7, 8)") qtquickTypeSuccess = false;
        m = "4,4,4,4,5,5,5,5,6,6,6,6,7,7,7,7";
        if (m != Qt.matrix4x4(4,4,4,4,5,5,5,5,6,6,6,6,7,7,7,7)) qtquickTypeSuccess = false;
        if (m.toString() != "QMatrix4x4(4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7)") qtquickTypeSuccess = false;
        c = "blue";
        if (c.toString() != Qt.rgba(0,0,1,0).toString()) qtquickTypeSuccess = false;
        if (c.toString() != "#0000FF" && c.toString() != "#0000ff") qtquickTypeSuccess = false; // color string converter is special
        // no string converter for fonts.
    }
}
