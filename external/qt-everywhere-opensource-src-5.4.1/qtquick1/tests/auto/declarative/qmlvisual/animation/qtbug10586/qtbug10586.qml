import QtQuick 1.0

/* This test checks that animations do occur while the flickable is flicking */
Rectangle {
    width: 200
    height: 400
    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: 1000; contentHeight: parent.height
        Rectangle {
            border.color: "black"
            border.width: 10
            width: 1000; height: 1000
        }
    }
    Rectangle {
        color: "red"
        width: 100; height: 100
        y: flick.contentX < 10 ? 300 : 0
        Behavior on y { NumberAnimation {} }
    }
}
