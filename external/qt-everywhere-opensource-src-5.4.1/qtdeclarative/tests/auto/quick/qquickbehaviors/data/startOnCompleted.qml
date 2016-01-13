import QtQuick 2.0

Rectangle {
    width: 400
    height: 400

    Rectangle {
        id: innerRect
        width: 100; height: 100
        color: "green"
        Behavior on x { NumberAnimation {} }
    }

    Component.onCompleted: innerRect.x = 100
}
