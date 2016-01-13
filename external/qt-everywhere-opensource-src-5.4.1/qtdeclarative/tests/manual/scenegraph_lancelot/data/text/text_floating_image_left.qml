import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        anchors.centerIn: parent
        font.pixelSize: 16
        width: parent.width
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        textFormat: Text.RichText
        text: "This image is to the<img align=left src=\"data/logo.png\" /> left of the text <br />More text"
    }
}
