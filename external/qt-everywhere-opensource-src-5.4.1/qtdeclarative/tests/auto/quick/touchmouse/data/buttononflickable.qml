import QtQuick 2.0
import Qt.test 1.0

Rectangle {
    id: root
    width: 300
    height: 500
    color: "green"

    Flickable {
        objectName: "flickable"
        anchors.fill: parent
        contentHeight: 1000

        Rectangle {
            objectName: "button"
            y: 100
            height: 100
            width: parent.width

            EventItem {
                objectName: "eventItem1"
                height: 100
                width: 300
            }
        }

        Rectangle {
            objectName: "button2"
            y: 300
            height: 100
            width: parent.width

            EventItem {
                objectName: "eventItem2"
                height: 100
                width: 300
            }
        }
    }
}

