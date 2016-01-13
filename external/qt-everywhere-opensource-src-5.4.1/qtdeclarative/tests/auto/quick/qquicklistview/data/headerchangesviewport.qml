import QtQuick 2.0

Rectangle {
    width: 240
    height: 320
    color: "#ffffff"
    ListView {
        id: list
        anchors.fill: parent
        objectName: "list"
        delegate: Text {}
        header: Text {
            objectName: "header"
            text: "ninjas"
            height: headerHeight
            width: headerWidth
        }
    }
}
