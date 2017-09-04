import QtQuick 2.0

Item
{
    width: 100
    height: 100

    Rectangle {
        id: box
        width: 600
        height: 600

        scale: 1 / 6.

        color: "black"

        layer.enabled: true
        layer.mipmap: true
        layer.smooth: true

        anchors.centerIn: parent

        Rectangle {
            x: 1
            width: 1
            height: parent.height
            color: "white"
        }
    }
}
