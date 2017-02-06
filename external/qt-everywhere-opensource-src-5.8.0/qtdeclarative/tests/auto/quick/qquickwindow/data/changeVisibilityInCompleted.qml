import QtQuick 2.0
import QtQuick.Window 2.1

Window {
    width: 200
    height: 200

    property var winVisible: subwin1
    property var winVisibility: subwin2

    Rectangle {
        anchors.fill: parent
        color:"green"
    }

    Window {
        id: subwin1
        width: 200
        height: 200

        visible: false

        Rectangle {
            anchors.fill: parent
            color:"red"
        }
        Component.onCompleted: {
            subwin1.visible = true
        }
    }
    Window {
        id: subwin2
        width: 200
        height: 200

        visible: true
        visibility: Window.Maximized

        Rectangle {
            anchors.fill: parent
            color:"blue"
        }
        Component.onCompleted: {
            subwin2.visibility = Window.Windowed
        }
    }
}
