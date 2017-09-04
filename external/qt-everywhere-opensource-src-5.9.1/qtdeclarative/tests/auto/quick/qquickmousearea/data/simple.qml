import QtQuick 2.0

Rectangle {
    id: whiteRect
    width: 200
    height: 200
    color: "white"
    MouseArea {
        objectName: "mousearea"
        anchors.fill: parent
    }
}
