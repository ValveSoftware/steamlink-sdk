import QtQuick 1.0

Rectangle {
    id: top
    width: 200; height: 70;

    property alias horizontalAlignment: text.horizontalAlignment
    property string text: "اختبا"

    Rectangle {
        anchors.centerIn: parent
        width: 180
        height: 20
        color: "green"

        TextInput {
            id: text
            objectName: "text"
            anchors.fill: parent
            text: top.text
            focus: true
        }
    }
}
