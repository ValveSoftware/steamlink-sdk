import QtQuick 1.0

Rectangle {
    id: wrapper
    width: 600
    height: 100

    Rectangle {
        id: redRect
        width: 100; height: 100
        color: Qt.rgba(1,0,0)
        /* This should produce an animation that starts at 0, animates smoothly
           to 100, jumps to 200, animates smoothly to 400, animates smoothly
           back to 100, jumps to 200, and so on.
        */
        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { to: 100; duration: 1000 }
            NumberAnimation { from: 200; to: 400; duration: 1000 }
        }

    }

}
