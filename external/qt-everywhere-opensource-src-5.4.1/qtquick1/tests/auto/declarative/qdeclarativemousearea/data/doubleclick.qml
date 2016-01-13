import QtQuick 1.0

Item {
    id: root
    property int clicked: 0
    property int doubleClicked: 0
    property int released: 0

    MouseArea {
        width: 200; height: 200
        onClicked: { root.clicked++ }
        onDoubleClicked: { root.doubleClicked++ }
        onReleased: { root.released++ }
    }
}

