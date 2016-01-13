import QtQuick 1.0

Rectangle {
    width: 200; height: 200
    Rectangle {
        objectName: "centered"
        width: 50; height: 50; color: "blue"
        anchors.centerIn: parent;
        anchors.verticalCenterOffset: 30
        anchors.horizontalCenterOffset: 10
    }
}
