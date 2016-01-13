import QtQuick 2.0

Rectangle {
    width: 320
    height: 480
    property bool smoothing: false

    Rectangle{
        id: rect_0_0
        width: 160
        height: 160
        x: 0
        y: 0
        Text {
            anchors.verticalCenter: parent.TopLeft
            text: "border size 1"
            z: 1
        }
        BorderImage {
            id: borderImage_1
            smooth: smoothing
            source: "../shared/sample_1.png"
            width: 120; height: 120
            border.left: 1; border.top: 1
            border.right: 1; border.bottom: 1
            anchors.centerIn: parent
        }
    }
    Rectangle{
        id: rect_0_1
        width: 160
        height: 160
        x: 160
        y: 0
        Text {
            anchors.verticalCenter: parent.TopLeft
            text: "border size 2"
            z: 1
        }
        BorderImage {
            id: borderImage_2
            smooth: smoothing
            source: "../shared/sample_1.png"
            width: 120; height: 120
            border.left: 2; border.top: 2
            border.right: 2; border.bottom: 2
            anchors.centerIn: parent
        }
    }
    Rectangle{
        id: rect_1_0
        width: 160
        height: 160
        x: 0
        y: 160
        Text {
            anchors.verticalCenter: parent.TopLeft
            text: "border size 3"
            z: 1
        }
        BorderImage {
            id: borderImage_3
            smooth: smoothing
            source: "../shared/sample_1.png"
            width: 120; height: 120
            border.left: 3; border.top: 3
            border.right: 3; border.bottom: 3
            anchors.centerIn: parent
        }
    }
    Rectangle{
        id: rect_1_1
        width: 160
        height: 160
        x: 160
        y: 160
        Text {
            anchors.verticalCenter: parent.TopLeft
            text: "border size 4"
            z: 1
        }
        BorderImage {
            id: borderImage_4
            smooth: smoothing
            source: "../shared/sample_1.png"
            width: 120; height: 120
            border.left: 4; border.top: 4
            border.right: 4; border.bottom: 4
            anchors.centerIn: parent
        }
    }
    Rectangle{
        id: rect_2_0
        width: 160
        height: 160
        x: 0
        y: 320
        Text {
            anchors.verticalCenter: parent.TopLeft
            text: "border size 5"
            z: 1
        }
        BorderImage {
            id: borderImage_5
            smooth: smoothing
            source: "../shared/sample_1.png"
            width: 120; height: 120
            border.left: 5; border.top: 5
            border.right: 5; border.bottom: 5
            anchors.centerIn: parent
        }
    }
    Rectangle{
        id: rect_2_1
        width: 160
        height: 160
        x: 160
        y: 320
        Text {
            anchors.verticalCenter: parent.TopLeft
            text: "border size 6"
            z: 1
        }
        BorderImage {
            id: borderImage_6
            smooth: smoothing
            source: "../shared/sample_1.png"
            width: 120; height: 120
            border.left: 6; border.top: 6
            border.right: 6; border.bottom: 6
            anchors.centerIn: parent
        }
    }
    Rectangle{
        id: rect_3_0
        width: 160
        height: 160
        x: 0
        y: 480
        Text {
            anchors.verticalCenter: parent.TopLeft
            text: "border size 7"
            z: 1
        }
        BorderImage {
            id: borderImage_7
            smooth: smoothing
            source: "../shared/sample_1.png"
            width: 120; height: 120
            border.left: 7; border.top: 7
            border.right: 7; border.bottom: 7
            anchors.centerIn: parent
        }
    }
    Rectangle{
        id: rect_3_1
        width: 160
        height: 160
        x: 160
        y: 480
        Text {
            anchors.verticalCenter: parent.TopLeft
            text: "border size 8"
            z: 1
        }
        BorderImage {
            id: borderImage_8
            smooth: smoothing
            source: "../shared/sample_1.png"
            width: 120; height: 120
            border.left: 8; border.top: 8
            border.right: 8; border.bottom: 8
            anchors.centerIn: parent
        }
    }


}
