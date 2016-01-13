import QtQuick 2.0

Item {
    width: 320
    height: 480

    TextEdit {
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 16
        textFormat: Text.RichText
        text: "<p>First line</p><p>Second <u>line</u></p><p><u>Third</u> line</p>"
    }

}
