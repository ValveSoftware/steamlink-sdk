import QtQuick 1.0

Rectangle {
    width: 400
    height: 200

    Item {
        objectName: "theItem"
        anchors.centerIn: parent
        width: childrenRect.width
        height: childrenRect.height
        Rectangle {
            id: text1
            anchors.verticalCenter: parent.verticalCenter
            width: 100; height: 100; color: "green"
        }
        Rectangle {
            anchors.left: text1.right
            anchors.verticalCenter: parent.verticalCenter
            width: 100; height: 100; color: "green"
        }
    }
}
