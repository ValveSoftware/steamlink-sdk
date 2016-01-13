import QtQuick 1.0
Rectangle {
    width: 400
    height: 400

    Item {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.bottom: blueHandle.top
        anchors.right: blueHandle.left

        Image {
            id: iconImage
            objectName: "iconImage"
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            source: "heart200.png"
            fillMode: Image.PreserveAspectFit
            smooth: true
        }
    }

    Rectangle {
        id: blueHandle
        objectName: "blueHandle"
        color: "blue"
        width: 25
        height: 25
    }
}
