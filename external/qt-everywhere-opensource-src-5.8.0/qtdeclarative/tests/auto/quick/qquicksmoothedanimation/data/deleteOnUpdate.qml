import QtQuick 2.0

Rectangle {
    width: 300; height: 300;

    Rectangle {
        color: "red"
        width: 60; height: 60;
        x: 100; y: 100;

        property real prevX: 100
        onXChanged: {
            if (x - prevX > 10) {
                anim.to += 5
                anim.restart(); //this can cause deletion of backend animation classes
                prevX = x;
            }
        }

        SmoothedAnimation on x {
            id: anim
            objectName: "anim"
            velocity: 100
            to: 150
        }
    }
}
