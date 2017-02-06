import QtQuick 2.4

Item {
    width: 500
    height: 500

    Rectangle {
        width: 300
        height: 300
        color: mouseArea.containsPress ? "red" : "grey"
        x: 100
        y: 100

        MouseArea {
            id: mouseArea
            objectName: "mouseArea"
            anchors.fill: parent
        }
    }
}
