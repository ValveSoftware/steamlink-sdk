import QtQuick 2.0

Rectangle {
    width: 320
    height: 480
    color: "#0c6d49"
    objectName: "top rect"

    Rectangle {
        id: greyRectangle
        objectName: "grey rect"
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
        height: parent.height / 2
        color: "grey"
    }

    MouseArea {
        objectName: "rear mouseArea"
        anchors.fill: parent
    }

    MouseArea {
        objectName: "front mouseArea"
        anchors.fill: greyRectangle
        onPressed: {
            mouse.accepted = false;
        }
    }
}
