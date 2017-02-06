import QtQuick 2.0
Rectangle {
    color: "skyblue"
    width: 500
    height: 200
    Rectangle {
        objectName: "cloud"
        color: "white"
        y: 50
        width: 100
        height: 100

        SequentialAnimation on x {
            loops: Animation.Infinite
            running: true
            NumberAnimation {
                id: firstAnimation
                from: 0
                to: 500
                duration: 5000
            }
            NumberAnimation {
                id: secondAnimation
                from: -100
                to: 0
                duration: 1000
            }
        }
    }
}
