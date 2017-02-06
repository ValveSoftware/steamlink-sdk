import QtQuick 2.0
import QtQuick.Window 2.0 as Windows

Windows.Window {

    width: 300
    height: 200

    Text {
        anchors.left: parent.left
        anchors.top: parent.top
        text: "Testing headless mode"
    }

    Rectangle {
        anchors.centerIn: parent
        width: 100
        height: 50
        rotation: -30
        gradient: Gradient {
            GradientStop { position: 0; color: "lightsteelblue" }
            GradientStop { position: 1; color: "black" }
        }
    }

    Image {
        source: "colors.png"
        anchors.bottom: parent.bottom
        anchors.right: parent.right
    }

}
