import QtQuick 1.0

Rectangle {
    id: root
    color: "#ffffff"
    width: 320; height: 240
    property bool mr1_pressed: false
    property bool mr1_released: false
    property bool mr1_canceled: false
    property bool mr2_pressed: false
    property bool mr2_released: false
    property bool mr2_canceled: false

    MouseArea {
        id: mouseRegion1
        anchors.fill: parent
        onPressed: { root.mr1_pressed = true }
        onReleased: { root.mr1_released = true }
        onCanceled: { root.mr1_canceled = true }
    }
    MouseArea {
        id: mouseRegion2
        width: 120; height: 120
        onPressed: { root.mr2_pressed = true; mouse.accepted = false }
        onReleased: { root.mr2_released = true }
        onCanceled: { root.mr2_canceled = true }
    }
}
