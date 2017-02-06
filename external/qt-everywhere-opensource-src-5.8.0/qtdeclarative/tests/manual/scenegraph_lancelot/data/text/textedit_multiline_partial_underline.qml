import QtQuick 2.0

Item {
    width: 320
    height: 480
    clip: true

    TextEdit {
        anchors.centerIn: parent
        font.pixelSize: 16
        textFormat: Text.RichText
        text: "First line<br />Second <u>line<br />Third</u> line"
    }

}
