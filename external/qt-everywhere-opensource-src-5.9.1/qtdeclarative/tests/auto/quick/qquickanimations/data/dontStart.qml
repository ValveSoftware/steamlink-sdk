import QtQuick 2.0

Rectangle {
    id: wrapper
    width: 600
    height: 400

    Rectangle {
        id: redRect
        width: 100; height: 100
        color: Qt.rgba(1,0,0)
        SequentialAnimation on x {
            running: false
            NumberAnimation { objectName: "MyAnim"; running: true }
        }

    }

}
