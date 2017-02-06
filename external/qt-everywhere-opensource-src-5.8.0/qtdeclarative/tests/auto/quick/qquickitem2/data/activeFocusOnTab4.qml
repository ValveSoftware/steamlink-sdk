import QtQuick 2.1

Item {
    id: main
    objectName: "main"
    width: 800
    height: 600
    focus: true
    Component.onCompleted: button11.focus = true
    Item {
        id: sub1
        objectName: "sub1"
        width: 230
        height: 600
        activeFocusOnTab: false
        anchors.top: parent.top
        anchors.left: parent.left
        Item {
            id: button11
            objectName: "button11"
            width: 100
            height: 50
            activeFocusOnTab: false
            Rectangle {
                anchors.fill: parent
                color: parent.activeFocus ? "red" : "black"
            }

            anchors.top: parent.top
            anchors.topMargin: 100
        }
    }
    Item {
        id: sub2
        objectName: "sub2"
        activeFocusOnTab: false
        width: 230
        height: 600
        anchors.top: parent.top
        anchors.left: sub1.right
        Item {
            id: button21
            objectName: "button21"
            width: 100
            height: 50
            activeFocusOnTab: true
            Rectangle {
                anchors.fill: parent
                color: parent.activeFocus ? "red" : "black"
            }

            anchors.top: parent.top
            anchors.topMargin: 100
        }
    }
}
