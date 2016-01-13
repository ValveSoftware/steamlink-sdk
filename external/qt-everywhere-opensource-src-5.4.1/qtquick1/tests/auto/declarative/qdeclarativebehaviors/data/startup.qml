import QtQuick 1.0

Rectangle {
    width: 400
    height: 400

    Rectangle {
        objectName: "innerRect"
        height: 100; width: 100; color: "green"
        property real targetX: 100

        x: targetX
        Behavior on x {
            NumberAnimation {}
        }
    }
}
