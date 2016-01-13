import QtQuick 1.1

Flickable {
    id: flick
    property real scale: 1.0
    width: 640
    height: 360
    contentWidth: 500
    contentHeight: 500

    PinchArea {
        objectName: "pincharea"
        width: Math.max(flick.contentWidth, flick.width)
        height: Math.max(flick.contentHeight, flick.height)

        property real initialWidth
        property real initialHeight
        onPinchStarted: {
            initialWidth = flick.contentWidth
            initialHeight = flick.contentHeight
        }

        onPinchUpdated: {
            // adjust content pos due to drag
            flick.contentX += pinch.previousCenter.x - pinch.center.x
            flick.contentY += pinch.previousCenter.y - pinch.center.y

            // resize content
            flick.resizeContent(initialWidth * pinch.scale, initialHeight * pinch.scale, pinch.center)
            flick.scale = pinch.scale
        }

        onPinchFinished: {
            // Move its content within bounds.
            flick.returnToBounds()
        }

        Rectangle {
            width: flick.contentWidth
            height: flick.contentHeight
            color: "white"
            Rectangle {
                anchors.centerIn: parent
                width: parent.width-40
                height: parent.height-40
                color: "blue"
            }
        }
    }
}
