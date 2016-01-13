import QtQuick 2.0

Rectangle {
    id: root

    property var angleDeltaY
    property real mouseX
    property real mouseY
    property bool controlPressed

    width: 400
    height: 400

    MouseArea {
        anchors.fill: parent

        onWheel: {
            root.angleDeltaY = wheel.angleDelta.y;
            root.mouseX = wheel.x;
            root.mouseY = wheel.y;
            root.controlPressed = wheel.modifiers & Qt.ControlModifier;
        }
    }
}
