import QtQuick 2.1

Item {
    id: main
    objectName: "main"
    width: 300
    height: 300
    Item {
        id: button1
        objectName: "button1"
        width: 300
        height: 150
        activeFocusOnTab: true
        Accessible.role: Accessible.Table
        Rectangle {
            anchors.fill: parent
            color: parent.activeFocus ? "red" : "black"
        }
        anchors.top: parent.top
        anchors.left: parent.left
    }
    Item {
        id: button2
        objectName: "button2"
        width: 300
        height: 150
        activeFocusOnTab: true
        Accessible.role: Accessible.Table
        Rectangle {
            anchors.fill: parent
            color: parent.activeFocus ? "red" : "black"
        }
        anchors.bottom: parent.bottom
        anchors.left: parent.left
    }
}
