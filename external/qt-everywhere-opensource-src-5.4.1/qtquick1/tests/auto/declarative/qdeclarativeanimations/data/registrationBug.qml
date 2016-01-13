import QtQuick 1.0

Rectangle {
    id: rect
    width: 200
    height: 200

    property bool animating: true
    property int value: 0

    NumberAnimation {
        target: rect
        property: "value"
        running: rect.animating
        to: 100
        duration: 50
    }
}
