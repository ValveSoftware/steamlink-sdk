import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 16
        textFormat: Qt.RichText
        text: "<p>First paragraph</p><p>Second paragraph</p><p>Third paragraph</p>"
    }

}
