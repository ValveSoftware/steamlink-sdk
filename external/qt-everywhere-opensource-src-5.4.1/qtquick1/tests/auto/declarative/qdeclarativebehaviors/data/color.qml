import QtQuick 1.0
Rectangle {
    width: 400
    height: 400
    Rectangle {
        id: rect
        objectName: "MyRect"
        width: 100; height: 100;
        color: "green"
        Behavior on color { ColorAnimation { duration: 500; } }
    }
    MouseArea {
        id: clicker
        anchors.fill: parent
    }
    states: State {
        name: "red"
        when: clicker.pressed
        PropertyChanges {
            target: rect
            color: "red"
        }
    }
}
