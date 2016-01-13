import QtQuick 2.0

Rectangle {
    property variant inputField: textedit
    height: 200
    width: 200

    Rectangle {
        height: parent.height /2
        width: parent.width
        color: "transparent"
        border.color: "black"
        border.width: 4
        Text {
            anchors.centerIn: parent
            height: parent.height * .95
            width: parent.width * .95
            text: textedit.text
        }
    }

    Rectangle {
        height: parent.height /2
        width: parent.width
        color: "transparent"
        border.color: "black"
        border.width: 4
        anchors.bottom: parent.bottom
        TextEdit {
            id: textedit
            anchors.centerIn: parent
            height: parent.height * .95
            width: parent.width * .95
            focus: true
            wrapMode: TextEdit.WordWrap
            text: ""
        }
    }
}
