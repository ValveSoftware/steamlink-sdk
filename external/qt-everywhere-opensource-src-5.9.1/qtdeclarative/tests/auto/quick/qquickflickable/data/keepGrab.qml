import QtQuick 2.3

Flickable {
    id: flickable
    width: 400
    height: 400
    contentWidth: 800
    contentHeight: 800

    Rectangle {
        x: 200
        y: 200
        width: 400
        height: 400
        color: "green"
        MouseArea {
            id: ma
            objectName: "ma"
            anchors.fill: parent
        }
    }
}
