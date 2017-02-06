import QtQuick 2.0

Item {
    property variant somepoint: Qt.point(1,2)
    property variant randomjsobj: {"some": 1, "thing": 2}
    property bool test1: somepoint != randomjsobj

    property variant similar: {"x":1, "y":2}
    property bool test2: somepoint != similar
}
