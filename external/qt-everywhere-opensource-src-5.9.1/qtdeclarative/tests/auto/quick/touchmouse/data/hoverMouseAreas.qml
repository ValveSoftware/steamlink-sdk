import QtQuick 2.4

Item {
    width: 500
    height: 500

    Rectangle {
        width: 300
        height: 90
        color: mouseArea1.containsMouse ? "red" : "grey"
        x: 100
        y: 100

        MouseArea {
            id: mouseArea1
            objectName: "mouseArea1"
            anchors.fill: parent
            hoverEnabled: true
            onPressed: parent.border.width = 4
            onReleased: parent.border.width = 0
        }
    }
    Rectangle {
        width: 300
        height: 100
        color: mouseArea2.containsMouse ? "red" : "lightblue"
        x: 100
        y: 200

        MouseArea {
            id: mouseArea2
            objectName: "mouseArea2"
            anchors.fill: parent
            hoverEnabled: true
            onPressed: parent.border.width = 4
            onReleased: parent.border.width = 0
        }
    }
}
