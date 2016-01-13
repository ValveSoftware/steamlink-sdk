import QtQuick 1.1

Rectangle {
    color: "green"
    width: loader.implicitWidth+50
    height: loader.implicitHeight+50

    Loader {
        id: loader
        sourceComponent: Item {
            anchors.centerIn: parent

            implicitWidth: 200
            implicitHeight: 200
            Rectangle {
                color: "red"
                anchors.fill: parent
            }
        }
        anchors.fill: parent
        anchors.margins: 15
    }
}
