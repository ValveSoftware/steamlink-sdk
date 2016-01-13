import QtQuick 1.0
Rectangle {
    id: container
    color: "red"
    height: 200
    width: 200
    Column {
        id: column
        objectName: "column"
        anchors.left: container.right
        anchors.bottom: container.bottom

        Rectangle {
            id: rectangle
            color: "blue"
            height: 100
            width: 200
        }
        Rectangle {
            color: "blue"
            height: 100
            width: 200
        }
    }
    states: State {
        name: "reanchored"
        AnchorChanges {
            target: column
            anchors.left: undefined
            anchors.right: container.right
        }
        PropertyChanges {
            target: rectangle
            opacity: 0
        }
    }
}
