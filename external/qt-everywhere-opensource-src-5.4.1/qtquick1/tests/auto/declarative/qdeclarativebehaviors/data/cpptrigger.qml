import QtQuick 1.0
Rectangle {
    width: 400
    height: 400
    Rectangle {
        id: rect
        objectName: "MyRect"
        width: 100; height: 100; color: "green"
        Behavior on x { NumberAnimation { duration: 500; } }
    }
}
