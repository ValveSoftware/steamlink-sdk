import QtQuick 1.0

Rectangle {
    color: "#ffffff"
    width: 320; height: 240

    Rectangle {
        id: rect
        color: "#00ff00"
        y: 200; width: 60; height: 20
        SequentialAnimation on y {
            loops: Animation.Infinite
            NumberAnimation {
                to: 20; duration: 500
                easing.type: "InOutQuad"
            }
            NumberAnimation {
                to: 200; duration: 2000
                easing.type: "OutBounce"
            }
            PauseAnimation { duration: 1000 }
        }
    }

    // Velocity
    Rectangle {
        color: "#ff0000"
        x: rect.width; width: rect.width; height: 20
        y: rect.y
        Behavior on y { SpringAnimation { velocity: 200 } }
    }

    // Spring
    Rectangle {
        color: "#ff0000"
        x: rect.width * 2; width: rect.width/2; height: 20
        y: rect.y
        Behavior on y { SpringAnimation { spring: 1.0; damping: 0.2 } }
    }
    Rectangle {
        color: "#880000"
        x: rect.width * 2.5; width: rect.width/2; height: 20
        y: rect.y
        Behavior on y { SpringAnimation { spring: 1.0; damping: 0.2; mass: 3.0 } } // "heavier" object
    }

    // Follow mouse
    MouseArea {
        id: mouseRegion
        anchors.fill: parent
        Rectangle {
            id: ball
            property int targetX: mouseRegion.mouseX - 10
            property int targetY: mouseRegion.mouseY - 10

            x: targetX
            y: targetY
            width: 20; height: 20
            radius: 10
            color: "#0000ff"

            Behavior on x { SpringAnimation { spring: 1.0; damping: 0.05; epsilon: 0.25 } }
            Behavior on y { SpringAnimation { spring: 1.0; damping: 0.05; epsilon: 0.25 } }

            states: [
                State {
                    name: "following"
                    when: ball.x != ball.targetX || ball.y != ball.targetY
                    PropertyChanges { target: ball; color: "#ff0000" }
                }
            ]
            transitions: [
                Transition {
                    ColorAnimation { duration: 200 }
                }
            ]
        }
    }
}
