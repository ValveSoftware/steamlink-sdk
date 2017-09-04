import QtQuick 2.0

Flickable {
    width: 400; height: 400
    contentWidth: 400; contentHeight: 1200

    property string signalString

    Rectangle {
        width: 400; height: 400
        color: "red"
    }
    Rectangle {
        y: 400; width: 400; height: 400
        color: "yellow"
    }
    Rectangle {
        y: 800; width: 400; height: 400
        color: "green"
    }

    onMovementStarted: signalString += "ms"
    onMovementEnded: signalString += "me"
    onFlickStarted: signalString += "fs"
    onFlickEnded: signalString += "fe"
}
