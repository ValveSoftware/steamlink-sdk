import QtQuick 2.0

Rectangle {
    id: top
    width: 70; height: 70;

    property alias horizontalAlignment: text.horizontalAlignment
    property string text: "Test"

    Rectangle {
        anchors.centerIn: parent
        width: 60
        height: 60
        color: "green"

        TextInput {
            objectName: "text"
            id: text
            anchors.fill: parent
            text: top.text
        }
    }
}
