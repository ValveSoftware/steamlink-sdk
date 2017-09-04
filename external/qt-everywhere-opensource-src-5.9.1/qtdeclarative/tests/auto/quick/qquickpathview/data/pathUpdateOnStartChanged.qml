import QtQuick 2.0

Rectangle {
    width: 800
    height: 480
    color: "black"
    resources: [
        ListModel {
            id: appModel
            ListElement { color: "green" }
        },
        Component {
            id: appDelegate
            Rectangle {
                id: wrapper
                objectName: "wrapper"
                color: "green"
                width: 100
                height: 100
            }
        }
    ]
    PathView {
        id: pathView
        objectName: "pathView"
        model: appModel
        anchors.fill: parent

        transformOrigin: "Top"
        delegate: appDelegate
        path: Path {
            objectName: "path"
            startX: pathView.width / 2 // startX: 400 <- this works as expected
            startY: 300
            PathLine { x: 400; y: 120 }
        }
    }
}
