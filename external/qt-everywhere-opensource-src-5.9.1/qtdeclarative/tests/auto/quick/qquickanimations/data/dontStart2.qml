import QtQuick 2.0

Rectangle {
    id: wrapper
    width: 600
    height: 400

    Rectangle {
        id: redRect
        width: 100; height: 100
        color: Qt.rgba(1,0,0)

        transitions: Transition {
            SequentialAnimation {
                NumberAnimation { id: myAnim; objectName: "MyAnim"; running: true }
            }
        }
    }
}
