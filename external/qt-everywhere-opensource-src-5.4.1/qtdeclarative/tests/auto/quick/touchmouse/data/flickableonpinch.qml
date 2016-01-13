import QtQuick 2.0
import Qt.test 1.0

Rectangle {
    id: root
    width: 600
    height: 600
    color: "green"

    PinchArea {
        objectName: "pincharea"
        pinch.target: rect
        anchors.fill: parent

        pinch.minimumScale: 1.0
        pinch.maximumScale: 10.0

        Flickable {
            objectName: "flickable"
            anchors.fill: parent
            contentHeight: 1000
            contentWidth: 1000

            Rectangle {
                objectName: "rect"
                id: rect
                color: "blue"
                x: 200
                y: 200
                width: 400
                height: 400
            }
        }
    }

}

