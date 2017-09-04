import QtQuick 2.0

QtObject {
    property variant test1: Qt.quaternion(2, 17, 0.9, 0.6);
    property variant test2: Qt.quaternion(102, -10, -982.1, 10);
    property variant test3: Qt.quaternion(102, -10, -982.1);
    property variant test4: Qt.quaternion(102, -10, -982.1, 10, 15);
}
