import QtQuick 2.0

Rectangle {
    id: top
    width: 200; height: 70;

    property alias horizontalAlignment: text.horizontalAlignment
    property string text: "اختبا"

    Rectangle {
        id: arabicContainer
        anchors.centerIn: parent
        width: 200
        height: 20
        color: "green"

        TextEdit {
            id: text
            objectName: "text"
            anchors.fill: parent
            text: top.text
            focus: true
            textFormat: TextEdit.AutoText
        }
    }

    Rectangle {
        anchors.top: arabicContainer.bottom
        anchors.left: arabicContainer.left
        width: 200
        height: 20
        color: "green"

        TextEdit {
            id: emptyTextEdit
            objectName: "emptyTextEdit"
            anchors.fill: parent
            textFormat: TextEdit.AutoText
        }
    }
}
