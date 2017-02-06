import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        anchors.centerIn: parent
        font.pixelSize: 16
        color: "blue"
        textFormat: Text.RichText
        text: "<p style=\"background-color:red\">Lorem ipsum</p><p style=\"background-color:green\">dolor sit amet,</p> consectetur."
    }
}
