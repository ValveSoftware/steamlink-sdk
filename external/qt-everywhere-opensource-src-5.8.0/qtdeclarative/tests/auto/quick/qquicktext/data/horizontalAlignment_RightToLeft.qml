import QtQuick 2.0

Rectangle {
    id: top
    width: 200; height: 70;

    property alias horizontalAlignment: text.horizontalAlignment
    property string text: "اختبا"

    Rectangle {
        anchors.centerIn: parent
        width: parent.width
        height: 20
        color: "green"

        Text {
            id: text
            objectName: "text"
            anchors.fill: parent
            text: top.text
        }
    }
}
