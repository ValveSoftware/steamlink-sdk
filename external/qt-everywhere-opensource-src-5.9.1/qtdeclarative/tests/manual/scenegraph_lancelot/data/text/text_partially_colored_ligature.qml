import QtQuick 2.0

Item {
    width: 320
    height: 480
    clip: true

    Text {
        id: text
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 20
        textFormat: Text.StyledText
        text: "Are grif<font color=\"#00ff00\">f</font>ins birds or mammals?"
    }

}
