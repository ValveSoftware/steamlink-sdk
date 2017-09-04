import QtQuick 2.0

Item {
    width: 800; height: 600
    Component {
        id: photoDelegate
        Package {
            Item { id: pathItem; objectName: "pathItem"; Package.name: 'path'; width: 85; height: 85; scale: pathItem.PathView.scale }
            Item { id: linearItem; Package.name: 'linear'; width: 85; height: 85 }
            Rectangle {
                id: wrapper
                width: 85; height: 85; color: lColor

                transform: Rotation {
                    id: itemRotation; origin.x: wrapper.width/2; origin.y: wrapper.height/2
                    axis.y: 1; axis.z: 0
                }
                state: 'path'
                states: [
                    State {
                        name: 'path'
                        ParentChange { target: wrapper; parent: pathItem; x: 0; y: 0 }
                        PropertyChanges { target: wrapper; opacity: pathItem.PathView.onPath ? 1.0 : 0 }
                    }
                ]
            }
        }
    }
    ListModel {
        id: rssModel
        ListElement { lColor: "red" }
        ListElement { lColor: "green" }
        ListElement { lColor: "yellow" }
        ListElement { lColor: "blue" }
        ListElement { lColor: "purple" }
        ListElement { lColor: "gray" }
        ListElement { lColor: "brown" }
        ListElement { lColor: "thistle" }
    }
    VisualDataModel { id: visualModel; model: rssModel; delegate: photoDelegate }

    PathView {
        id: photoPathView
        objectName: "photoPathView"
        width: 800; height: 330; pathItemCount: 4; offset: 1
        dragMargin: 24
        preferredHighlightBegin: 0.50
        preferredHighlightEnd: 0.50

        path: Path {
            startX: -50; startY: 40;

            PathAttribute { name: "scale"; value: 0.5 }
            PathAttribute { name: "angle"; value: -45 }

            PathCubic {
                x: 400; y: 220
                control1X: 140; control1Y: 40
                control2X: 210; control2Y: 220
            }

            PathAttribute { name: "scale"; value: 1.2  }
            PathAttribute { name: "angle"; value: 0 }

            PathCubic {
                x: 850; y: 40
                control2X: 660; control2Y: 40
                control1X: 590; control1Y: 220
            }

            PathAttribute { name: "scale"; value: 0.5 }
            PathAttribute { name: "angle"; value: 45 }
        }

        model: visualModel.parts.path
    }

    PathView {
        y: 400; width: 800; height: 330; pathItemCount: 8

        path: Path {
            startX: 0; startY: 40;
            PathLine { x: 800; y: 40 }
        }

        model: visualModel.parts.linear
    }
}
