import QtQuick 2.0
import Qt.test 1.0

Rectangle {
    id: root
    width: 320
    height: 480
    color: "green"

    EventItem {
        objectName: "eventItem1"
        height: 200
        width: 100

        EventItem {
            objectName: "eventItem2"
            height: 100
            width: 100
        }
    }
}

