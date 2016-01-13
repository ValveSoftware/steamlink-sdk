import QtQuick 2.0

// positioning via anchors contaminated with a binding
Rectangle {
    id: p
    color: "red"
    width: 400
    height: 800

    Rectangle {
        id: c
        color: "blue"
        anchors.top: p.verticalCenter
        anchors.bottom: p.bottom
        anchors.left: p.left
        anchors.right: p.right

        Rectangle {
            id: n1
            color: "green"
            height: c.height / 4
            anchors.top: c.verticalCenter
            anchors.left: c.left
            anchors.right: c.horizontalCenter
        }

        Rectangle {
            id: n2
            color: "cyan"
            anchors.top: c.verticalCenter
            anchors.bottom: n1.bottom
            anchors.left: c.horizontalCenter
            anchors.right: c.right
        }

        Rectangle {
            id: n3
            color: "aquamarine"
            anchors.top: n1.bottom
            anchors.bottom: c.bottom
            anchors.left: c.left
            anchors.right: c.horizontalCenter
        }

        Rectangle {
            id: n4
            color: "lightgreen"
            anchors.top: n1.bottom
            anchors.bottom: c.bottom
            anchors.left: c.horizontalCenter
            anchors.right: c.right
        }
    }

    // for visually determining correctness.
    //Timer {
    //    property int count: 0
    //    interval: 1000
    //    running: true
    //    repeat: true
    //    onTriggered: {
    //        if (count == 0) {
    //            count = 1;
    //            p.height = 800;
    //        } else {
    //            count = 0;
    //            p.height = 400;
    //        }
    //    }
    //}

    Component.onCompleted: {
        p.height = 800;
        p.height = 400;
    }
}
