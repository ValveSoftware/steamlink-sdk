import QtQuick 2.0

Flickable {
    id: outer
    objectName: "flickable"
    width: 400
    height: 400
    contentX: 50
    contentY: 50
    contentWidth: 500
    contentHeight: 500
    boundsBehavior: Flickable.StopAtBounds

    Rectangle {
        x: 100
        y: 100
        width: 300
        height: 300

        color: "yellow"
    }
}
