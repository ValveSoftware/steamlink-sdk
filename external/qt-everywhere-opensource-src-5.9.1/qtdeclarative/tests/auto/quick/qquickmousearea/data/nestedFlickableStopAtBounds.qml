import QtQuick 2.5


Rectangle {
    width: 240
    height: 320

    MouseArea {
        objectName: "mouseArea"
        anchors.fill: parent

        drag.target: moveable
        drag.filterChildren: true

        Rectangle {
            id: moveable
            objectName: "moveable"
            color: "red"
            x: 50
            y: 80
            width: 200
            height: 200
        }

        Flickable {
            objectName: "flickable"
            anchors.fill: parent
            contentHeight: 450

            Rectangle {
                x: 100
                y: 50
                width: 50
                height: 350
                color: "yellow"
            }

            MouseArea {
                width: 240
                height: 450
            }
        }
    }
}
