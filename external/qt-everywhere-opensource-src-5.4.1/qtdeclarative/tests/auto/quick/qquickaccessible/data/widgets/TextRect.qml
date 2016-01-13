import QtQuick 2.0

Rectangle {
    id: button

    property alias text : buttonText.text
    Accessible.name: text
    Accessible.description: "This button does " + text
    Accessible.role: Accessible.Client

    signal clicked

    width: 40
    height: 40
    border.width: 2
    border.color: "black";

    Text {
        id: buttonText
        text: "TextRect"
        anchors.centerIn: parent
        font.pixelSize: parent.height * .1
        style: Text.Sunken; color: "white"; styleColor: "black"; smooth: true
    }

}
