import QtQuick 2.1

Rectangle {
    MouseArea {
        anchors.fill: parent;
        Component.onCompleted: {
            update();
        }
    }
}
