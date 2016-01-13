import QtQuick 1.0
Item { //realWindow
    width: 370
    height: 480
    Item {
        id: window
        NumberAnimation on width{ from:320; to:370; duration: 500 }
        NumberAnimation on height{ from:480; to:320; duration: 500 }
        Flipable {
            id: flipable
            x: 0
            y: window.height / 3.0 - 40
            width: parent.width
            height: parent.height
            transform: Rotation {
                id: transform
                origin.x: window.width / 2.0
                origin.y: 0
                origin.z: 0
                axis.x: 0
                axis.y: 1
                axis.z: 0
                angle: 0;
            }
            front: Rectangle{
                width: parent.width
                height: 80
                color: "blue"
            }back: Rectangle{
                width: parent.width
                height: 80
                color: "red"
            }
        }
        Flipable {
            id: flipableBack
            x: 0
            y: 2.0 * window.height / 3.0 - 40
            width: parent.width
            height: parent.height
            transform: Rotation {
                id: transformBack
                origin.x: window.width / 2.0
                origin.y: 0
                origin.z: 0
                axis.x: 0
                axis.y: 1
                axis.z: 0
                angle: 180;
            }
            front: Rectangle{
                width: parent.width
                height: 80
                color: "blue"
            }back: Rectangle{
                width: parent.width
                height: 80
                color: "red"
            }
        }
    }
}
