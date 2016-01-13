import QtQuick 1.0

Rectangle {
    id: clock
    color: "gray"
    width: 200; height: 200

    property variant hours: 10
    property variant minutes: 28
    property variant seconds: 0

    Timer {
        interval: 1000; running: true; repeat: true; triggeredOnStart: true
        onTriggered: seconds++
    }

    Image { id: background; source: "content/clock.png" }

    Image {
        x: 92.5; y: 27
        source: "content/hour.png"
        transform: Rotation {
            id: hourRotation
            origin.x: 7.5; origin.y: 73
            angle: (clock.hours * 30) + (clock.minutes * 0.5)

            Behavior on angle {
                SpringAnimation { spring: 2; damping: 0.2; modulus: 360 }
            }
        }
    }

    Image {
        x: 93.5; y: 17
        source: "content/minute.png"
        transform: Rotation {
            id: minuteRotation
            origin.x: 6.5; origin.y: 83
            angle: clock.minutes * 6

            Behavior on angle {
                SpringAnimation { spring: 2; damping: 0.2; modulus: 360 }
            }
        }
    }

    Image {
        x: 97.5; y: 20
        source: "content/second.png"
        transform: Rotation {
            id: secondRotation
            origin.x: 2.5; origin.y: 80
            angle: clock.seconds * 6

            Behavior on angle {
                SpringAnimation { spring: 5; damping: 0.25; modulus: 360 }
            }
        }
    }

    Image {
        anchors.centerIn: background; source: "content/center.png"
    }
}
