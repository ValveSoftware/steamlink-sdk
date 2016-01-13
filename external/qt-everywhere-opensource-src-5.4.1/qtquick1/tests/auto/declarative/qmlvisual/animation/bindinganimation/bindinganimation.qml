import QtQuick 1.0

Rectangle {
    color: "blue"
    width: 320
    height: 240
    id: page
    Rectangle {
        id: myRectangle
        width: 100
        height: 100
        color: "red"
        x: 10
    }
    states: [
        State {
            when: myMouseArea.pressed
            name: "hello"
            PropertyChanges {
                target: myRectangle
                x: 50 + 50
            }
        }
    ]
    transitions: [
        Transition {
            NumberAnimation {
                properties: "x"
            }
        }
    ]
    MouseArea {
        id: myMouseArea
        anchors.fill: parent
    }
}
