import QtQuick 2.0
import Test 1.0

Flickable {
    width: 240
    height: 320
    contentWidth: width * 1.5
    contentHeight: height * 1.5
    contentY: height * 0.25

    Rectangle {
        id: slider
        width: 50
        height: 200
        color: "lightgray"
        border.color: drag.active ? "green" : "black"
        anchors.centerIn: parent
        radius: 4

        TouchDragArea {
            id: drag
            objectName: "drag"
            anchors.fill: parent
        }

        Rectangle {
            width: parent.width - 2
            height: 20
            radius: 5
            color: "darkgray"
            border.color: "black"
            x: 1
            y: Math.min(slider.height - height, Math.max(0, drag.pos.y - height / 2))
        }
    }
}
