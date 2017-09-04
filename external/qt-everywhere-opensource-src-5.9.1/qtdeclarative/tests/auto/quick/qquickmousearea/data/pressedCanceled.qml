import QtQuick 2.0
import QtQuick.Window 2.0

Rectangle {
    id: root
    color: "#ffffff"
    width: 320; height: 240
    property bool pressed:mouse.pressed
    property bool canceled: false
    property int clicked: 0
    property int doubleClicked: 0
    property int released: 0
    property alias secondWindow: secondWindow

    Window {
        id: secondWindow
        x: root.x + root.width
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        onPressed: { root.canceled = false }
        onCanceled: {root.canceled = true}
        onReleased: {root.released++; root.canceled = false}
        onClicked: {root.clicked++}
        onDoubleClicked: {root.doubleClicked++}
    }
}
