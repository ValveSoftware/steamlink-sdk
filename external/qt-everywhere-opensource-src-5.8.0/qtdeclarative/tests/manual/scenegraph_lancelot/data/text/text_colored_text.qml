import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        anchors.centerIn: parent
        font.pixelSize: 16
        color: "blue"
        textFormat: Text.RichText
        text: "<font color=\"red\">Lorem ipsum</font> <font color=\"green\">dolor sit amet<font>, consectetur."
    }
}
