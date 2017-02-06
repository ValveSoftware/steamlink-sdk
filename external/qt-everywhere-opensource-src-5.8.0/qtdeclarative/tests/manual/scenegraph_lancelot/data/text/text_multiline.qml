import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 16
        textFormat: Qt.RichText
        text: "First line<br />Second line<br />Third line"
    }

}
