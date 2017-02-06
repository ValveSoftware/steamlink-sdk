import QtQuick 2.0

Flipable {
    id: flipable
    width: 640; height: 480
    property bool flipped: false

    front: Rectangle { color: "red"; anchors.fill: flipable }
    back: Rectangle { color: "blue"; anchors.fill: flipable }

    transform: Rotation {
        id: rotation
        origin.x: flipable.width/2
        origin.y: flipable.height/2
        axis.x: 0; axis.y: 1; axis.z: 0     // set axis.y to 1 to rotate around y-axis
        angle: 0    // the default angle
    }

    states: State {
        name: "back"
        PropertyChanges { target: rotation; angle: 540 }
        when: flipable.flipped
    }

    transitions: Transition {
        NumberAnimation { target: rotation; property: "angle"; duration: 500 }
    }
}
