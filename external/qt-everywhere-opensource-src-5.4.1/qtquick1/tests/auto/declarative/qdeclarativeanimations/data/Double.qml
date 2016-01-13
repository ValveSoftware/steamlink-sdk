import QtQuick 1.0

Rectangle {
    id: container
    property bool on: false
    border.color: "#ffffff"
    color: "green"
    width: 50
    height: 50
    NumberAnimation on x {
        objectName: "animation"
        running: container.on; from: 0; to: 600; loops: Animation.Infinite; duration: 2000
    }
}
