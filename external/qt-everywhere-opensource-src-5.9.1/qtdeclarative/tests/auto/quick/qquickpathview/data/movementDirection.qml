import QtQuick 2.0

Item {
    id: root
    width: 320; height: 480

    PathView {
        id: view
        objectName: "view"
        anchors.fill: parent

        model:  ListModel {
            ListElement { lColor: "red" }
            ListElement { lColor: "green" }
            ListElement { lColor: "yellow" }
            ListElement { lColor: "blue" }
            ListElement { lColor: "purple" }
            ListElement { lColor: "gray" }
            ListElement { lColor: "brown" }
            ListElement { lColor: "thistle" }
        }

        delegate: Component {
            id: photoDelegate
            Rectangle {
                id: wrapper
                objectName: "wrapper"
                width: 40; height: 40; color: lColor

                Text { text: index }
            }
        }

        snapMode: PathView.SnapToItem
        highlightMoveDuration: 1000
        path:  Path {
            startX: 0+20; startY: root.height/2
            PathLine { x: root.width*2; y: root.height/2 }
        }

        Text { text: "Offset: " + view.offset }
    }
}
