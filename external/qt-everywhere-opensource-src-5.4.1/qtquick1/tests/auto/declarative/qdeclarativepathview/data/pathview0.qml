import QtQuick 1.0

Rectangle {
    id: root
    property int count: view.count
    property int currentA: -1
    property int currentB: -1
    property real delegateWidth: 60
    property real delegateHeight: 20
    property real delegateScale: 1.0
    width: 240
    height: 320
    color: "#ffffff"
    resources: [
        Component {
            id: delegate
            Rectangle {
                id: wrapper
                objectName: "wrapper"
                height: root.delegateHeight
                width: root.delegateWidth
                scale: root.delegateScale
                color: PathView.isCurrentItem ? "lightsteelblue" : "white"
                border.color: "black"
                Text {
                    text: index
                }
                Text {
                    x: 20
                    id: textName
                    objectName: "textName"
                    text: name
                }
                Text {
                    x: 40
                    id: textNumber
                    objectName: "textNumber"
                    text: number
                }
                PathView.onCurrentItemChanged: {
                    if (PathView.isCurrentItem) {
                        root.currentA = index;
                        root.currentB = wrapper.PathView.view.currentIndex;
                    }
                }
            }
        }
    ]
    PathView {
        id: view
        objectName: "view"
        width: 240
        height: 320
        model: testModel
        delegate: delegate
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
                control2Y: 000
                control2X: 200
            }
        }
    }
}
