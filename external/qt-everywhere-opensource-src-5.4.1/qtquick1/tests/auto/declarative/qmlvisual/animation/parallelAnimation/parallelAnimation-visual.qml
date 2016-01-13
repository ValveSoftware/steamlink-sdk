import QtQuick 1.0

/*
    This test verifies that a single animation animating two properties is visually the same as two
    animations in a parallel group animating the same properties. Visually, you should see a red
    rectangle at 0,0 stretching from the top of the window to the bottom. This rect will be moved to
    the right side of the window while turning purple. If the bottom half is visually different
    than the top half, there is a problem.
*/

Rectangle {
    width: 200; height: 100
    Rectangle {
        id: redRect
        width: 50; height: 50
        color: "red"
    }
    Rectangle {
        id: redRect2
        width: 50; height: 50
        y: 50
        color: "red"
    }

    Timer{
        interval: 100
        running: true
        onTriggered: parent.state = "state1"
    }

    states: State {
        name: "state1"
        PropertyChanges {
            target: redRect
            x: 150
            color: "purple"
        }
        PropertyChanges {
            target: redRect2
            x: 150
            color: "purple"
        }
    }

    transitions: Transition {
        PropertyAnimation { targets: redRect; properties: "x,color"; duration: 300 }
        ParallelAnimation {
            NumberAnimation { targets: redRect2; properties: "x"; duration: 300 }
            ColorAnimation { targets: redRect2; properties: "color"; duration: 300 }
        }
    }
}
