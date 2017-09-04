import QtQuick 2.0

Item {
    width: 500
    height: 500

    Rectangle {
        width: 300
        height: 300
        color: "grey"
        x: 100
        y: 100

        MouseArea {
            id: mouseArea
            objectName: "mouseArea"
            anchors.fill: parent
            hoverEnabled: true
            onPressed: mouse.accepted = false
            //onContainsMouseChanged: print("containsMouse changed =", containsMouse)

            Rectangle {
                visible: parent.containsMouse
                color: "red"
                width: 10; height: 10
            }
        }
    }
}
