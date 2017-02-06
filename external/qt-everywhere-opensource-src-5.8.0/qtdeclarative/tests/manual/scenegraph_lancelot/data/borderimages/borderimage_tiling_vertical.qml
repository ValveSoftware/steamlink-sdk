import QtQuick 2.0

Rectangle {
    width: 320
    height: 480
    property bool smoothing: true
    Rectangle{
        id: rect_0_0
        width: 160
        height: 160
        x: 0
        y: 0

        BorderImage {
            id: borderImage_0
            smooth: smoothing
            source: "../shared/sample_1.png"
            width: 120; height: 120
            border.left: 10; border.top: 10
            border.right: 10; border.bottom: 10
            verticalTileMode: BorderImage.Stretch
            anchors.centerIn: parent
        }
        Text {
            anchors.top: borderImage_0.bottom
            text: "mode: stretch"
            z: 1
        }
    }
    Rectangle{
        id: rect_0_1
        width: 160
        height: 160
        x: 160
        y: 0

        BorderImage {
            id: borderImage_1
            smooth: smoothing
            source: "../shared/sample_1.png"
            width: 120; height: 120
            border.left: 10; border.top: 10
            border.right: 10; border.bottom: 10
            verticalTileMode: BorderImage.Repeat
            anchors.centerIn: parent
        }
        Text {
            anchors.top: borderImage_1.bottom
            text: "mode: repeat"
            z: 1
        }
    }
    Rectangle{
        id: rect_1_0
        width: 160
        height: 160
        x: 0
        y: 160

        BorderImage {
            id: borderImage_2
            smooth: smoothing
            source: "../shared/sample_1.png"
            width: 120; height: 120
            border.left: 10; border.top: 10
            border.right: 10; border.bottom: 10
            verticalTileMode: BorderImage.Round
            anchors.centerIn: parent
        }
        Text {
            anchors.top: borderImage_2.bottom
            text: "mode: round"
            z: 1
        }
    }
}
