import QtQuick 2.0

Rectangle {
    width: 500
    height: 500

    AnimatedImage {
        objectName: "anim"
        anchors.centerIn: parent
        asynchronous: true
        opacity: status == AnimatedImage.Ready ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: 1000 }
        }
    }
}
