import QtQuick 2.0

Item {
    TextInput {
        objectName: "input"

        focus: true
        width: -1
        height: -1
        text: "sushi"

        anchors {
            left: parent.left;
            leftMargin: 5
            top: parent.top
            topMargin: font.pixelSize
        }
    }
}
