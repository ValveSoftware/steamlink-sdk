import QtQuick 2.0

Flickable {
    property bool stealing: true
    width: 200
    height: 200
    contentWidth: 400
    contentHeight: 400
    Rectangle {
        color: "black"
        width: 400
        height: 400
        Rectangle {
            x: 50; y: 50
            width: 100; height: 100
            color: "steelblue"
            MouseArea {
                objectName: "mousearea"
                anchors.fill: parent
                preventStealing: stealing
            }
        }
    }
}
