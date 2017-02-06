import QtQuick 2.0

//compare default font sizes
Item {
    width: 320
    height: 480
    Text {
        id: text_0000
        anchors.top: parent.top
        anchors.left: parent.left
        text: "The quick brown fox jumps over the lazy dog. 0123456789"
        font.family: "Arial"
        font.pixelSize: 14
    }
    TextEdit {
        id: text_0001
        anchors.top: text_0000.bottom
        anchors.left: parent.left
        text: "The quick brown fox jumps over the lazy dog. 0123456789"
        font.family: "Arial"
        font.pixelSize: 14
    }
    TextInput {
        id: text_0002
        anchors.top: text_0001.bottom
        anchors.left: parent.left
        text: "The quick brown fox jumps over the lazy dog. 0123456789"
        font.family: "Arial"
        font.pixelSize: 14
    }
}
