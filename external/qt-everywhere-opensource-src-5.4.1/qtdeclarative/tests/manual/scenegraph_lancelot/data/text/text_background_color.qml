import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        anchors.centerIn: parent
        font.pixelSize: 16
        color: "green"
        textFormat: Text.RichText
        text: "<body bgcolor=lightblue>Some text<br />Some more text</body>"
    }
}
