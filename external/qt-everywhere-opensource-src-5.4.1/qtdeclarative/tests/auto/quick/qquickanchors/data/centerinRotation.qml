import QtQuick 2.0

Rectangle {
    width: 200; height: 200
    Rectangle {
        objectName: "outer"
        rotation: 90
        width: 101; height: 101; color: "blue"
        anchors.centerIn: parent;
        anchors.alignWhenCentered: false

        Rectangle {
            objectName: "inner"
            width: 50; height: 50; color: "blue"
            anchors.centerIn: parent;
        }
    }
}
