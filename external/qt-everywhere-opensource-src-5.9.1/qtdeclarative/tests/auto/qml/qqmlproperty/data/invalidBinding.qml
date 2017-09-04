import QtQuick 2.0

Item {
    property Text text: myText

    property Rectangle rectangle1: myText
    property Rectangle rectangle2: getMyText()

    function getMyText() { return myText; }

    Text {
        id: myText

        anchors.verticalCenter: parent // invalid binding
    }
}
