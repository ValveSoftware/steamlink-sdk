import QtQuick 2.0

PathView {
    width: 500
    height: 500

    model: 2
    path: Path {
        startX: -300
        startY: 200
        PathLine {
            x: 700
            y: 200
        }
    }
    delegate: Rectangle {
        width: 300
        height: 300
        border.width: 5
        color: "lightsteelblue"

        Rectangle {
            x: 100
            y: 100
            width: 100
            height: 100

            color: "yellow"

            MouseArea {
                drag.target: parent
                anchors.fill: parent
            }
        }
    }
}
