import QtQuick 2.0
import Qt.test 1.0

Rectangle {
    id: root
    width: 320
    height: 480
    color: "green"

    EventItem {
        id: leftItem
        objectName: "leftItem"
        width: root.width / 2
        height: root.height
    }

    EventItem {
        objectName: "rightItem"
        anchors.left: leftItem.right
        width: root.width / 2
        height: root.height
    }
}

