import QtQuick 1.0

Flipable {
    id: flipable
    width: 640; height: 480

    front: Rectangle { anchors.fill: flipable }
    back: Rectangle { anchors.fill: flipable }
}
