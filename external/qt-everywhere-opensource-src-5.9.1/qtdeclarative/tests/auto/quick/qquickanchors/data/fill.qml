import QtQuick 2.0

Rectangle {
    width: 200; height: 200
    Rectangle {
        objectName: "filler"
        width: 50; height: 50; color: "blue"
        anchors.fill: parent;
        anchors.leftMargin: 10;
        anchors.rightMargin: 20;
        anchors.topMargin: 30;
        anchors.bottomMargin: 40;
    }
}
