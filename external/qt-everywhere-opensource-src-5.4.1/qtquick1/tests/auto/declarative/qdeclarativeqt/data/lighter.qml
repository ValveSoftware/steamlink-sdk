import QtQuick 1.0

QtObject {
    property variant test1: Qt.lighter(Qt.rgba(1, 0.8, 0.3))
    property variant test2: Qt.lighter()
    property variant test3: Qt.lighter(Qt.rgba(1, 0.8, 0.3), 1.8)
    property variant test4: Qt.lighter("red");
    property variant test5: Qt.lighter("perfectred"); // Non-existent color
    property variant test6: Qt.lighter(10);
    property variant test7: Qt.lighter(Qt.rgba(1, 0.8, 0.3), 1.8, 5)
}
