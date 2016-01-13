import QtQuick 2.0

Rectangle {
    property real implWidth: 0
    property real implHeight: 0
    function changeImplicitSize () {
        loader.item.implicitWidth = 200
        loader.item.implicitHeight = 300
    }
    color: "green"
    width: loader.implicitWidth+50
    height: loader.implicitHeight+50

    Loader {
        id: loader
        objectName: "loader"
        sourceComponent: Item {
            anchors.centerIn: parent

            implicitWidth: 100
            implicitHeight: 100
            Rectangle {
                color: "red"
                anchors.fill: parent
            }
        }

        anchors.fill: parent
        anchors.margins: 50
        onImplicitWidthChanged: implWidth = implicitWidth
        onImplicitHeightChanged: implHeight = loader.implicitHeight
    }
}
