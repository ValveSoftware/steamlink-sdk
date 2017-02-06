import QtQuick 2.0

QtObject {
    property bool qtquickTypeSuccess: false

    // currently, only conversion from js object to font and matrix is supported.
    property matrix4x4 m: Qt.matrix4x4(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16)
    property matrix4x4 m2: Qt.matrix4x4([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16])
    property font f: Qt.font({ family: "Arial", pointSize: 10, weight: Font.Bold, italic: true })
    property font f2: Qt.font({ family: "Arial", pointSize: 10, weight: Font.Bold, italic: true })

    Component.onCompleted: {
        qtquickTypeSuccess = true;

        // check that the initialisation worked
        if (m != m2) qtquickTypeSuccess = false;
        if (f != f2) qtquickTypeSuccess = false;

        // check that assignment works
        m = Qt.matrix4x4(1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4)
        m2 = Qt.matrix4x4([1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4])
        if (m != m2) qtquickTypeSuccess = false;
        f = Qt.font({ family: "Arial", pointSize: 16, weight: Font.Black, italic: false });
        f2 = Qt.font({ family: "Arial", pointSize: 16, weight: Font.Black, italic: false });
        if (f != f2) qtquickTypeSuccess = false;

        // ensure that equality works as required.
        if (m2 != Qt.matrix4x4([1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4])) qtquickTypeSucces = false;
        if (f2 != Qt.font({ family: "Arial", pointSize: 16, weight: Font.Black, italic: false })) qtquickTypeSuccess = false;

        // just to ensure comparison of values from js object assigned values is consistent.
        m = Qt.matrix4x4(5,5,5,5,2,2,2,2,3,3,3,3,4,4,4,4);
        m2 = Qt.matrix4x4([6,6,6,6,2,2,2,2,3,3,3,3,4,4,4,4]);
        if (m == m2) qtquickTypeSuccess = false;
        m = Qt.matrix4x4(6,6,6,6,2,2,2,2,3,3,3,3,4,4,4,4);
        if (m != m2) qtquickTypeSuccess = false;
        m = Qt.matrix4x4([7,7,7,7,2,2,2,2,3,3,3,3,4,4,4,4]);
        if (m == m2) qtquickTypeSuccess = false;
        m = Qt.matrix4x4([6,6,6,6,2,2,2,2,3,3,3,3,4,4,4,4]);
        if (m != m2) qtquickTypeSuccess = false;

        f = Qt.font({ family: "Arial", pointSize: 10, weight: Font.Bold, italic: true });
        f2 = Qt.font({ family: "Arial", pointSize: 16, weight: Font.Black, italic: false });
        if (f == f2) qtquickTypeSuccess = false;
        f = Qt.font({ family: "Arial", pointSize: 16, weight: Font.Black, italic: false });
        if (f != f2) qtquickTypeSuccess = false;
    }
}
