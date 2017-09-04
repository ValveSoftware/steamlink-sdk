import QtQuick 2.0
import Test 1.0

Rectangle {
    id: root
    width: 160
    height: 240
    color: "black"
    Rectangle {
        width: root.width
        height: root.height / 2;
        anchors.centerIn: parent
        clip: true
        color: "white"
        Rectangle {
            width: root.width / 2;
            height: root.height / 2
            anchors.centerIn: parent
            rotation: 45
            color: "blue"
            clip: true
            MessUpItem {
                anchors.fill: parent
            }
            Rectangle {
                anchors.fill: parent
                anchors.margins: -100
                color: "red"
                opacity: 0.5
            }
        }
    }
}
