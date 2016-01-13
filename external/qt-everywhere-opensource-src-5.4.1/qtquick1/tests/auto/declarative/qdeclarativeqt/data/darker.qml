import QtQuick 1.0

QtObject {
    property variant test1: Qt.darker(Qt.rgba(1, 0.8, 0.3))
    property variant test2: Qt.darker()
    property variant test3: Qt.darker(Qt.rgba(1, 0.8, 0.3), 2.8)
    property variant test4: Qt.darker("red");
    property variant test5: Qt.darker("perfectred"); // Non-existent color
    property variant test6: Qt.darker(10);
    property variant test7: Qt.darker(Qt.rgba(1, 0.8, 0.3), 2.8, 10)
}

