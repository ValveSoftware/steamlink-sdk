import QtQuick 2.0

Flickable {
    property bool pressed: ma.pressed
    width: 240
    height: 320
    contentWidth: 480
    contentHeight: 320
    flickableDirection: Flickable.HorizontalFlick
    pressDelay: 50
    Rectangle {
        anchors.fill: parent
        anchors.margins: 50
        color: "yellow"

    }

    onMovingChanged: console.log("outer moving", moving)

    Flickable {
        objectName: "innerFlickable"
        anchors.fill: parent
        flickableDirection: Flickable.VerticalFlick
        contentWidth: 480
        contentHeight: 1480
        pressDelay: 50
        Rectangle {
            anchors.fill: parent
            anchors.margins: 100
            color: ma.pressed ? 'blue' : 'green'
        }
        MouseArea {
            id: ma
            objectName: "mouseArea"
            anchors.fill: parent
        }
    }
}

