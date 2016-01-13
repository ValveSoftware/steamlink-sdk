import QtQuick 2.0

Rectangle {
    width: 400; height: 400;

    Rectangle {
        width: 100; height: 100;
        MouseArea {
            id: mousetracker; objectName: "mousetracker"
            anchors.fill: parent
            visible: false
            hoverEnabled: true
        }
    }
}
