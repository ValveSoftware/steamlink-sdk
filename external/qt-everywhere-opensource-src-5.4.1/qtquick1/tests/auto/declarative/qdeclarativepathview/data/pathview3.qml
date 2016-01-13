import QtQuick 1.0

PathView {
    id: photoPathView
    y: 100; width: 800; height: 330; pathItemCount: 4; offset: 1
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

    model: ListModel {
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

    delegate: Component {
        id: photoDelegate
        Rectangle {
            id: wrapper
            width: 85; height: 85; color: lColor

            transform: Rotation {
                id: itemRotation; origin.x: wrapper.width/2; origin.y: wrapper.height/2
                axis.y: 1; axis.z: 0
            }
        }
    }
}
