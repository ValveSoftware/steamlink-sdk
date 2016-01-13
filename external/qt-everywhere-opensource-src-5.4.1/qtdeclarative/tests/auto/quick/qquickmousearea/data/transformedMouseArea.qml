import QtQuick 2.0

Rectangle {
    width: 400
    height: 400

    Rectangle {
        x: 100
        y: 100
        width: 200
        height: 200
        rotation: 45

        MouseArea {
            scale: 0.5
            hoverEnabled: true
            anchors.fill: parent
            objectName: "mouseArea"
        }
    }
}
