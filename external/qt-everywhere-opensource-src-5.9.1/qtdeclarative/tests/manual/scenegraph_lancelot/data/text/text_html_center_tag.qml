import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        id: implicitSizedText
        textFormat: Text.RichText
        text: "<center>Implicit size<br>----- Second line -----</center>"
        anchors.centerIn: parent
        color: "white"

        Rectangle {
            anchors.fill: parent
            z: -1
            color: "blue"
        }
    }
    Text {
        textFormat: Text.RichText
        text: "<center>Explicit size<br>----- Second line -----</center>"
        anchors.top: implicitSizedText.bottom
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        width: 300
        color: "white"

        Rectangle {
            anchors.fill: parent
            z: -1
            color: "blue"
        }
    }
}
