import QtQuick 2.0
Rectangle {
    width: 400
    height: 400
    property real basex : 0
    property real movedx: 200
    Rectangle {
        id: rect
        objectName: "MyRect"
        width: 100; height: 100; color: "green"
        x: basex
        Behavior on x { NumberAnimation { duration: 800; } }
    }
    MouseArea {
        id: clicker
        anchors.fill: parent
    }
    states: State {
        name: "moved"
        when: clicker.pressed
        PropertyChanges {
            target: rect
            x: movedx
        }
    }
}
