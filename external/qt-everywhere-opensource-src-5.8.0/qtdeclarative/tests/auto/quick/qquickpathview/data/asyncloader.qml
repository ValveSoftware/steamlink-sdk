import QtQuick 2.0

Rectangle {
    id: root
    property real delegateWidth: 60
    property real delegateHeight: 20
    property real delegateScale: 1.0
    width: 240
    height: 320
    color: "#ffffff"

    Loader {
        asynchronous: true
        sourceComponent: viewComponent
        anchors.fill: parent
    }

    Component {
        id: adelegate
        Rectangle {
            objectName: "wrapper"
            property bool onPath: PathView.onPath
            height: root.delegateHeight
            width: root.delegateWidth
            scale: root.delegateScale
            color: PathView.isCurrentItem ? "lightsteelblue" : "white"
            border.color: "black"
            Text {
                text: index
            }
        }
    }
    Component {
        id: viewComponent
        PathView {
            id: view
            objectName: "view"
            width: 240
            height: 320
            model: 5
            delegate: adelegate
            highlight: Rectangle {
                width: 60
                height: 20
                color: "yellow"
            }
            path: Path {
                startY: 120
                startX: 160
                PathQuad {
                    y: 120
                    x: 80
                    controlY: 330
                    controlX: 100
                }
                PathLine {
                    y: 160
                    x: 20
                }
                PathCubic {
                    y: 120
                    x: 160
                    control1Y: 0
                    control1X: 100
                    control2Y: 0
                    control2X: 200
                }
            }
        }
    }
}
