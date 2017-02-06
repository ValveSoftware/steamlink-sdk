import QtQuick 2.7

Item {
    id: root
    // by default you can flick via touch or tablet but not via mouse
    property int allowedSource: Qt.MouseEventNotSynthesized
    property int lastEventSource: -1
    width: 200
    height: 200
    Flickable {
        objectName: "flickable"
        anchors.fill: parent
        contentWidth: 400
        contentHeight: 400
        Rectangle {
            color: ma.pressed ? "yellow" : "steelblue"
            width: 200
            height: 200
        }
    }
    MouseArea {
        id: ma
        objectName: "mousearea"
        onPressed: {
            root.lastEventSource = mouse.source
            if (mouse.source !== root.allowedSource)
                mouse.accepted = false
        }
        anchors.fill: parent
    }
}
