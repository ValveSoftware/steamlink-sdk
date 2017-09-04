import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 16
        textFormat: Text.RichText
        text: "This image is <img width=16 height=16 src=\"data/logo.png\" /> in the middle of the text"
    }
}
