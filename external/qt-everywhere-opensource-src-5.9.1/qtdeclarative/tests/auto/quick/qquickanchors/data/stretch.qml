import QtQuick 2.0

Rectangle {
    width: 400; height: 400

    Rectangle {
        id: rect1
        x: 20; y: 20;
        height: 360; width: 360;

        Rectangle {
            id: rect2; objectName: "stretcher"
            anchors.verticalCenter: rect1.verticalCenter
            anchors.bottom: rect3.top
            anchors.horizontalCenter: rect1.horizontalCenter
            anchors.left: rect3.left
        }

        Rectangle {
            id: rect3
            x: 160; y: 230
            width: 10
            height: 10
        }

        Rectangle {
            id: rect4; objectName: "stretcher2"
            anchors.verticalCenter: rect1.verticalCenter
            anchors.top: rect5.top
        }

        Rectangle {
            id: rect5
            x: 160; y: 130
            width: 10
            height: 10
        }
    }
}
