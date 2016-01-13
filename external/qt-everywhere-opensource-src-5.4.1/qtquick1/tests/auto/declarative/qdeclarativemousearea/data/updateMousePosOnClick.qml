import QtQuick 1.0

Rectangle {
    color: "#ffffff"
    width: 320; height: 240
    MouseArea {
        id: mouseRegion
        objectName: "mouseregion"
        anchors.fill: parent
        Rectangle {
            id: ball
            objectName: "ball"
            width: 20; height: 20
            radius: 10
            color: "#0000ff"
            x: { mouseRegion.mouseX }
            y: mouseRegion.mouseY
        }
    }
}
