import QtQuick 1.0

/*
This test starts with a red rectangle at 0,0. It should animate moving 50 pixels right,
then immediately change blue, and then animate moving 50 pixels down.
*/

Rectangle {
    width: 100; height: 100
    Rectangle {
        id: myRect
        width: 50; height: 50
        color: "red"
    }
    MouseArea {
        id: clickable
        anchors.fill: parent
    }

    states: State {
        name: "state1"
        when: clickable.pressed
        PropertyChanges {
            target: myRect
            x: 50; y: 50
        }
        StateChangeScript {
            name: "setColor"
            script: myRect.color = "blue"
        }
    }

    transitions: Transition {
        SequentialAnimation {
            NumberAnimation { properties: "x"; easing.type: "InOutQuad" }
            ScriptAction { scriptName: "setColor" }
            NumberAnimation { properties: "y"; easing.type: "InOutQuad" }
        }
    }
}
