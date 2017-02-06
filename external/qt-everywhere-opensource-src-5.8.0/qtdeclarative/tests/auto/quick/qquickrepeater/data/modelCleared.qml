import QtQuick 2.0

Row {
    spacing: 2
    height: 100

    Repeater {
        id: repeater
        objectName: "repeater"
        model: 10
        Rectangle {
            color: "green"
            width: 10; height: 50
            anchors.bottom: parent.bottom
        }
    }
}
