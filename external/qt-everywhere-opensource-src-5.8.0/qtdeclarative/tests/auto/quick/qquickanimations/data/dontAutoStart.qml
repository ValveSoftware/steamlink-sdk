import QtQuick 2.0

Rectangle {
    id: wrapper
    width: 600
    height: 400

    Rectangle {
        id: redRect
        width: 100; height: 100
        color: Qt.rgba(1,0,0)
        Behavior on x {
            NumberAnimation { id: myAnim; objectName: "MyAnim"; target: redRect; property: "y"; to: 300; loops: Animation.Infinite}
        }

    }

}
