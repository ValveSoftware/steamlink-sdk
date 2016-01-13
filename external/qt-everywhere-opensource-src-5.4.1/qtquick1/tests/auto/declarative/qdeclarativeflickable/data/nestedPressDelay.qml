import QtQuick 1.0

Flickable {
    property bool pressed: ma.pressed
    width: 240
    height: 320
    contentWidth: 480
    contentHeight: 320
    flickableDirection: Flickable.HorizontalFlick
    pressDelay: 50
    Flickable {
        objectName: "innerFlickable"
        flickableDirection: Flickable.VerticalFlick
        width: 480
        height: 320
        contentWidth: 480
        contentHeight: 400
        pressDelay: 10000
        Rectangle {
            y: 100
            anchors.horizontalCenter: parent.horizontalCenter
            width: 240
            height: 100
            color: ma.pressed ? 'blue' : 'green'
            MouseArea {
                id: ma
                objectName: "mouseArea"
                anchors.fill: parent
            }
        }
    }
}

