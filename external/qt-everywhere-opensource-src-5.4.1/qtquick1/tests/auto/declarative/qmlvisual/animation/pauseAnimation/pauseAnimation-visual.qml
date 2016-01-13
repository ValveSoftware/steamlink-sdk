import QtQuick 1.0

/*
This test shows a bouncing logo.
When the test starts the logo should be resting at the bottom. It should immediately move
to the top, and then fall down to bounce at the bottom. There should be a pause, and then
one repeat of the sequence.
*/

Rectangle {
    id: rect
    width: 120
    height: 200
    color: "white"
    Image {
        id: img
        source: "pics/qtlogo.png"
        x: 60-width/2
        y: 200-img.height
        SequentialAnimation on y {
            loops: Animation.Infinite
            NumberAnimation {
                to: 0; duration: 500
                easing.type: "InOutQuad"
            }
            NumberAnimation {
                to: 200-img.height
                easing.type: "OutBounce"
                duration: 2000
            }
            PauseAnimation {
                duration: 1000
            }
        }
    }
}
