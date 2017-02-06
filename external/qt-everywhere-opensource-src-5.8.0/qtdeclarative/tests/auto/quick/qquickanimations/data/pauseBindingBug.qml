import QtQuick 2.0

Rectangle {
    id: rect
    width: 200
    height: 200

    property bool animating: false
    property int value: 0

    NumberAnimation on value {
        objectName: "animation"
        paused: !rect.animating
        to: 100
        duration: 50
    }
}
