import QtQuick 1.0

/*
This test shows a green rectangle moving and growing from the upper-left corner
of the black rectangle to the same position as the red rectangle (it should end up
the same height as the red rect and twice as wide). There should be no odd jumps or clipping seen.

The test shows one full transition (to the red and back), then several partial transitions, and
then a final full transition.
*/

Rectangle {
    width: 400;
    height: 240;
    color: "black";

    Rectangle {
        id: gr
        color: "green"
        width: 50; height: 50
    }

    MouseArea {
        id: mouser
        anchors.fill: parent
    }

    Rectangle {
        id: np
        x: 150
        width: 150; height: 150
        color: "yellow"
        clip: true
        Rectangle {
            color: "red"
            x: 50; y: 50; height: 50; width: 50
        }

    }

    Rectangle {
        id: vp
        x: 100; y: 100
        width: 50; height: 50
        color: "blue"
        rotation: 45
        scale: 2
    }

    states: State {
        name: "state1"
        when: mouser.pressed
        ParentChange {
            target: gr
            parent: np
            x: 50; y: 50; width: 100;
        }
    }

    transitions: Transition {
        reversible: true
        to: "state1"
        ParentAnimation {
            target: gr; via: vp;
            NumberAnimation { properties: "x,y,rotation,scale,width" }
        }
    }
}
