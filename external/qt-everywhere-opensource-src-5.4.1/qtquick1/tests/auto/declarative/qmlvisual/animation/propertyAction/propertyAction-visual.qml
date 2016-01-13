import QtQuick 1.0

/*
This test starts with a 30x40 rectangle at 0,0. It should animate a width change to 40,
then jump 50 pixels right, and then animate moving 50 pixels down. Afer this it should
do an exact visual reversal (animate up 50 pixels, jump left 50 pixels, and then animate
a change back to 30px wide).
*/

Rectangle {
    width: 100; height: 100
    Rectangle {
        id: myRect
        width: 30; height: 40
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
            x: 50; y: 50; width: 40
        }
    }

    transitions: Transition {
        to: "state1"
        reversible: true
        SequentialAnimation {
            NumberAnimation { properties: "width"; easing.type: "InOutQuad" }
            PropertyAction { properties: "x" }
            NumberAnimation { properties: "y"; easing.type: "InOutQuad" }
        }
    }
}
