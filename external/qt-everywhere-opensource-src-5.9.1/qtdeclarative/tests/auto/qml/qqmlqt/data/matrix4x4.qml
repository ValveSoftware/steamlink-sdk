import QtQuick 2.0

QtObject {
    property variant test1: Qt.matrix4x4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    property variant test2: Qt.matrix4x4([1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4]);
    property variant test3: Qt.matrix4x4(1,2,3,4,5,6);
    property variant test4: Qt.matrix4x4([1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5]);
    property variant test5: Qt.matrix4x4({ test: 5, subprop: "hello" });
}
