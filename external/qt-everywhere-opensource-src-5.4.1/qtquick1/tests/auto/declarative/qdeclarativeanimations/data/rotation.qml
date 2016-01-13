import QtQuick 1.0

Rectangle {
    width: 600; height: 200

    Row {
        spacing: 5
        Rectangle {
            id: rr
            objectName: "rr"
            color: "red"
            width: 100; height: 100
        }
        Rectangle {
            id: rr2
            objectName: "rr2"
            color: "red"
            width: 100; height: 100
        }
        Rectangle {
            id: rr3
            objectName: "rr3"
            color: "red"
            width: 100; height: 100
        }
        Rectangle {
            id: rr4
            objectName: "rr4"
            color: "red"
            width: 100; height: 100
        }
    }

    states: State {
        name: "state1"
        PropertyChanges { target: rr; rotation: 370 }
        PropertyChanges { target: rr2; rotation: 370 }
        PropertyChanges { target: rr3; rotation: 370 }
        PropertyChanges { target: rr4; rotation: 370 }
    }

    transitions: Transition {
        RotationAnimation { target: rr; direction: RotationAnimation.Numerical; duration: 1000 }
        RotationAnimation { target: rr2; direction: RotationAnimation.Clockwise; duration: 1000 }
        RotationAnimation { target: rr3; direction: RotationAnimation.Counterclockwise; duration: 1000 }
        RotationAnimation { target: rr4; direction: RotationAnimation.Shortest; duration: 1000 }
    }
}
