import QtQuick 2.0

Item {
    id: root
    property bool clicked: false
    property bool held: false

    MouseArea {
        width: 200; height: 200
        onClicked: { root.clicked = true }
        onPressAndHold: { root.held = true }
    }
}
