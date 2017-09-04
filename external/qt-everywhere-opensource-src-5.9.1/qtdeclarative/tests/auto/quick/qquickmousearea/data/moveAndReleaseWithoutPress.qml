import QtQuick 2.0

MouseArea {
    width: 200
    height: 200

    property bool hadMove: false
    property bool hadRelease: false

    onPressed: mouse.accepted = false
    onPositionChanged: hadMove = true
    onReleased: hadRelease = true
}

