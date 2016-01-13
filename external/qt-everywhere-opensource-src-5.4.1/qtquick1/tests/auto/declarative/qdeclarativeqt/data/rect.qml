import QtQuick 1.0

QtObject {
    property variant test1: Qt.rect(10, 13, 100, 109)
    property variant test2: Qt.rect(-10, 13, 100, 109.6)
    property variant test3: Qt.rect(10, 13);
    property variant test4: Qt.rect(10, 13, 100, 109, 10)
    property variant test5: Qt.rect(10, 13, 100, -109)
}
