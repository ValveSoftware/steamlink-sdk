import QtQuick 2.3

Rectangle {
    property alias value: range.width
    color: "yellow"
    Text {
        text: 'value: ' + value
    }

    Rectangle {
        id: range
        objectName: "range"
        color: "red"
        width: 0
        height: 5
        anchors.bottom: parent.bottom
    }
}
