import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 16
        textFormat: Qt.RichText
        font.underline: true
        text: "<p>First line</p><p>Second line</p><p>Third line</p>"
    }

}
