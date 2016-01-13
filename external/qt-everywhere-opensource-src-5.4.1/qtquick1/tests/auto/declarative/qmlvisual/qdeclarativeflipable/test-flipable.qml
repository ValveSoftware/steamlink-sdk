import QtQuick 1.0

Rectangle {
    width: 400; height: 240
    color: "white"

    Timer {
        interval: 3000; running: true; repeat: true; triggeredOnStart: true
        onTriggered: {
            if (flipable.state == '') flipable.state = 'back'; else flipable.state = ''
            if (flipable2.state == '') flipable2.state = 'back'; else flipable2.state = ''
        }
    }

    Flipable {
        id: flipable
        width: 200; height: 200

        transform: Rotation {
            id: rotation; angle: 0
            origin.x: 100; origin.y: 100
            axis.x: 0; axis.y: 1; axis.z: 0
        }

        front: Rectangle {
            color: "steelblue"; width: 200; height: 200
        }

        back: Rectangle {
            color: "deeppink"; width: 200; height: 200
        }

        states: State {
            name: "back"
            PropertyChanges { target: rotation; angle: 180 }
        }

        transitions: Transition {
            NumberAnimation { easing.type: "InOutQuad"; properties: "angle"; duration: 3000 }
        }
    }

    Flipable {
        id: flipable2
        x: 200; width: 200; height: 200

        transform: Rotation {
            id: rotation2; angle: 0
            origin.x: 100; origin.y: 100
            axis.x: 1; axis.z: 0
        }

        front: Rectangle {
            color: "deeppink"; width: 200; height: 200
        }

        back: Rectangle {
            color: "steelblue"; width: 200; height: 200
        }

        states: State {
            name: "back"
            PropertyChanges { target: rotation2; angle: 180 }
        }

        transitions: Transition {
            NumberAnimation { easing.type: "InOutQuad"; properties: "angle"; duration: 3000 }
        }
    }

    Rectangle {
        x: 25; width: 150; y: 210; height: 20; color: "black"
        visible: flipable.side == Flipable.Front
    }
    Rectangle {
        x: 225; width: 150; y: 210; height: 20; color: "black"
        visible: flipable2.side == Flipable.Back
    }
}
