import QtQuick 2.0

Flickable {
    width: 320
    height: 480
    contentWidth: c1.width
    contentHeight: c1.height

    Rectangle {
        id: c1
        width: 500
        height: 1000
        color: "blue"
    }
}
