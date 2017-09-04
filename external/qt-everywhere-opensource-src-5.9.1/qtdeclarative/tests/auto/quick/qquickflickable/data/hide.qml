import QtQuick 2.0

Flickable {
    id: flickable
    width: 200; height: 200
    contentWidth: 400; contentHeight: 400

    MouseArea {
        objectName: "mouseArea"
        width: 400; height: 400
        onDoubleClicked: flickable.visible = false
    }
}
