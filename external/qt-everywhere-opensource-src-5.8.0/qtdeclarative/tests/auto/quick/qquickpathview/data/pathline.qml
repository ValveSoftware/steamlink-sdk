import QtQuick 2.0

Rectangle {
    id: app
    width: 360
    height: 360

    PathView {
        id: pathView
        objectName: "view"
        x: (app.width-pathView.width)/2
        y: 100
        width: 240
        height: 100

        model: testModel

        Rectangle {
            anchors.fill: parent
            color: "white"
            border.color: "black"
        }
        preferredHighlightBegin: 0.5
        preferredHighlightEnd: 0.5

        delegate: Rectangle {
            objectName: "wrapper"
            width: 100
            height: 100
            color: PathView.isCurrentItem ? "red" : "yellow"
            Text {
                text: index
                anchors.centerIn: parent
            }
            z: (PathView.isCurrentItem?1:0)
        }
        path: Path {
            id: path
            startX: -100+pathView.width/2
            startY: pathView.height/2
            PathLine {
                id: line
                x: 100+pathView.width/2
                y: pathView.height/2
            }
        }
    }
}
