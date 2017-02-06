import QtQuick 2.0

Rectangle {
    width: 800
    height: 800

    Rectangle {
        id: redRect; objectName: "redRect"
        color: "red"
        width: 50; height: 50
        x: 500; y: 50
    }

    states: State {
        name: "moved"
        PropertyChanges {
            target: redRect
            x: 100; y: 700
        }
    }

    transitions: Transition {
        to: "moved"; reversible: true
        PathAnimation {
            id: pathAnim
            target: redRect
            duration: 300
            path: Path {
                PathCurve { x: 100; y: 100 }
                PathCurve { x: 200; y: 350 }
                PathCurve { x: 600; y: 500 }
                PathCurve {}
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: parent.state = parent.state == "moved" ? "" : "moved"
    }
}
