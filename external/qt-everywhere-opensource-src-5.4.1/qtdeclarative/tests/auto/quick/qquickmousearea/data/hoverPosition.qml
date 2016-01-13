import QtQuick 2.0

Rectangle {
    width: 400; height: 400;

    property real mouseX: mousetracker.mouseX
    property real mouseY: mousetracker.mouseY

    Rectangle {
        width: 100; height: 100;
        MouseArea {
            id: mousetracker;
            anchors.fill: parent;
            hoverEnabled: true
        }
    }
}
