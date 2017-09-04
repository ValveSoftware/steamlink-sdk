import QtQuick 2.0
import Qt.test 1.0

Rectangle {
    id: root

    width: 600
    height: 400

    EventItem {
        objectName: "background"
        width: 600
        height: 400
        Rectangle { anchors.fill: parent; color: "lightsteelblue" }

        EventItem {
            objectName: "left"
            width: 300
            height: 300
            Rectangle { anchors.fill: parent; color: "yellow" }
        }

        EventItem {
            objectName: "right"
            x: 300
            width: 300
            height: 300
            Rectangle { anchors.fill: parent; color: "green" }
        }

        EventItem {
            objectName: "middle"
            x: 150
            width: 300
            height: 300
            Rectangle { anchors.fill: parent; color: "blue" }
        }
    }
}
