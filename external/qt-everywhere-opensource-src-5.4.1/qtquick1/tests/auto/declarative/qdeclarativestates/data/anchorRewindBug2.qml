import QtQuick 1.0

Rectangle {
    id: root
    width:200; height:300

    Rectangle {
        id: rectangle
        objectName: "mover"
        color: "green"
        width:50; height:50
    }

    states: [
        State {
            name: "anchored"
            AnchorChanges {
                target: rectangle
                anchors.left: root.left
                anchors.right: root.right
                anchors.bottom: root.bottom
            }
        }
    ]
}
