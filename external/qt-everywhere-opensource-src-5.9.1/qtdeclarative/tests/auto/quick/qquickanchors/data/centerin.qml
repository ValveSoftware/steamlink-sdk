import QtQuick 2.0

Rectangle {
    width: 200; height: 200
    Rectangle {
        objectName: "centered"
        width: 50; height: 50; color: "blue"
        anchors.centerIn: parent;
        anchors.verticalCenterOffset: 30
        anchors.horizontalCenterOffset: 10
    }

    Rectangle {
        objectName: "centered2"
        width: 11; height: 11; color: "green"
        anchors.centerIn: parent;
    }

    Rectangle {
        objectName: "centered3"
        width: 11; height: 11; color: "green"
        anchors.centerIn: parent;
        anchors.alignWhenCentered: false
    }
}
