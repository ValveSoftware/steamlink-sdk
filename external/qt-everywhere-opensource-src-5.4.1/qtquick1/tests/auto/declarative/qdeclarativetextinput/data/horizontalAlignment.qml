import QtQuick 1.0

Rectangle {
    id: top
    width: 70; height: 70;

    property alias horizontalAlignment: text.horizontalAlignment
    property string text: "Test"

    Rectangle {
        anchors.centerIn: parent
        width: 60
        height: 20
        color: "green"

        TextInput {
            id: text
            anchors.fill: parent
            text: top.text
        }
    }
}
