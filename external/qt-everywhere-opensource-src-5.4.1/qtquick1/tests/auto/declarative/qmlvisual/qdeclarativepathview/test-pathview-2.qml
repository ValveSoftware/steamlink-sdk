import QtQuick 1.0

Rectangle {
    width: 580; height: 220
    //Same as test-pathview, but with pathItemCount < model.count

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

    Component {
        id: photoDelegate
        Rectangle {
            id: wrapper
            width: 65; height: 65; color: lColor
            scale: wrapper.PathView.scale

            MouseArea { anchors.fill: parent }

            transform: Rotation {
                id: itemRotation; origin.x: wrapper.width/2; origin.y: wrapper.height/2
                axis.y: 1; axis.z: 0; angle: wrapper.PathView.angle
            }
        }
    }

    PathView {
        id: photoPathView; model: rssModel; delegate: photoDelegate
        anchors.fill: parent;  z: 1
        anchors.topMargin:40
        pathItemCount: 6
        path: Path {
            startX: -50; startY: 40;

            PathAttribute { name: "scale"; value: 0.5 }
            PathAttribute { name: "angle"; value: -45 }

            PathCubic {
                x: 300; y: 140
                control1X: 90; control1Y: 30
                control2X: 140; control2Y: 150
            }

            PathAttribute { name: "scale"; value: 1.2  }
            PathAttribute { name: "angle"; value: 0 }

            PathCubic {
                x: 600; y: 30
                control2X: 440; control2Y: 30
                control1X: 420; control1Y: 150
            }

            PathAttribute { name: "scale"; value: 0.5 }
            PathAttribute { name: "angle"; value: 45 }
        }
    }

    Column {
        Rectangle { width: 20; height: 20; color: "red"; opacity: photoPathView.moving ? 1 : 0 }
        Rectangle { width: 20; height: 20; color: "blue"; opacity: photoPathView.flicking ? 1 : 0 }
    }
}
