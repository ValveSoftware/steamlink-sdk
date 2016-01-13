import QtQuick 2.0

Rectangle {
    width: 200; height: 200
    Rectangle {
        objectName: "filler"
        width: 50; height: 50; color: "blue"
        anchors.fill: parent;
        anchors.margins: 10
        anchors.leftMargin: 5
        anchors.topMargin: 6
    }
}
