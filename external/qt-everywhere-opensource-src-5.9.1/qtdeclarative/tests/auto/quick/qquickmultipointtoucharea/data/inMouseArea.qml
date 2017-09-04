import QtQuick 2.2

MouseArea {
    id: root
    width: 240
    height: 320
    acceptedButtons: Qt.LeftButton | Qt.RightButton

    Rectangle {
        anchors.fill: parent
        color: "black"
        border.width: 5
        border.color: parent.pressed ? "red" : "blue"

        Rectangle {
            anchors.fill: parent
            anchors.margins: 25
            color: mpta.pressed ? "orange" : "cyan"

            MultiPointTouchArea {
                id: mpta
                objectName: "mpta"
                property bool pressed: false
                anchors.fill: parent
                onPressed: pressed = true
                onReleased: pressed = false
            }
        }
    }
}
