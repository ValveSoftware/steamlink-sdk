import QtQuick 2.0

Item {
    id: root
    property int clicked: 0
    property int pressed: 0
    property int released: 0

    MouseArea {
        objectName: "mousearea"
        width: 200; height: 200
        onPressed: { root.pressed++ }
        onClicked: { root.clicked++ }
        onReleased: { root.released++ }
    }
}

