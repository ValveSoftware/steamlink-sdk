import QtQuick 2.0
import Qt.test 1.0

Rectangle {
    id: root
    width: 320
    height: 480
    color: "green"

    EventItem {
        objectName: "eventItem1"
        x: 5
        y: 5
        height: 30
        width: 30
    }
}

