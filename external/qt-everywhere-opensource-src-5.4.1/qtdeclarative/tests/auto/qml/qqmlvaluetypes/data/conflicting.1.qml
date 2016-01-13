import QtQuick 2.0

Rectangle {
    id: root

    width: 800
    height: 600

    property alias font: myText.font

    property int myPixelSize: 12
    property int myPixelSize2: 24

    Text {
        id: other
        font.pixelSize: 6
    }

    Text {
        id: myText

        text: "Hello world!"
        font.pixelSize: myPixelSize
    }

    states: State {
        name: "Swapped"
        PropertyChanges {
            target: myText
            font: other.font
        }
    }

    function toggle() {
        if (root.state == "") root.state = "Swapped"; else root.state = "";
    }

    MouseArea {
        anchors.fill: parent
        onClicked: { if (root.state == "") root.state = "Swapped"; else root.state = "";}
    }
}
