import QtQuick 2.0

QtObject {
    property color test1: Qt.tint("red", "blue");
    property color test2: Qt.tint(Qt.rgba(1, 0, 0), Qt.rgba(0, 0, 0, 0));
    property color test3: Qt.tint("red", Qt.rgba(0, 0, 1, 0.5));
    property color test4: Qt.tint("red", Qt.rgba(0, 0, 1, 0.5), 10);
    property color test5: Qt.tint("red")
}
