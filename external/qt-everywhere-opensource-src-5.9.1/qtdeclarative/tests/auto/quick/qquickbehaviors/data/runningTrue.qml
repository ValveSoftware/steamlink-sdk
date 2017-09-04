import QtQuick 2.0

Rectangle {
    id: root
    width:200; height:200

    property real myValue: 0

    Rectangle {
        anchors.centerIn: parent
        width: 100
        height: 100
        color: "green"
        smooth: true
        rotation: myValue
        Behavior on rotation {
            RotationAnimation { id: rotAnim; objectName: "rotAnim"; direction: RotationAnimation.Shortest }
        }
    }
}
