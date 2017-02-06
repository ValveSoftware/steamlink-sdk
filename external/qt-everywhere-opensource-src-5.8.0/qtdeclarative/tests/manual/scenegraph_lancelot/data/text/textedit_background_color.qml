import QtQuick 2.0

Item {
    width: 320
    height: 480

    TextEdit {
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 16
        color: "green"
        textFormat: Text.RichText
        text: "<body bgcolor=lightblue>Some text<br />Some more text</body>"
    }
}
