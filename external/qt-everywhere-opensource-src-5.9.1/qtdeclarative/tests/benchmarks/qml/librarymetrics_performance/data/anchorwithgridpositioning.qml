import QtQuick 2.0

// positioning via anchors with a grid
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

        Grid {
            id: g
            rows: 2
            columns: 2
            anchors.top: c.verticalCenter
            anchors.bottom: c.bottom
            anchors.left: c.left
            anchors.right: c.right

            Rectangle {
                id: n1
                color: "green"
                anchors.top: g.top
                anchors.bottom: g.verticalCenter
                anchors.left: g.left
                anchors.right: g.horizontalCenter
            }

            Rectangle {
                id: n2
                color: "cyan"
                anchors.top: g.top
                anchors.bottom: g.verticalCenter
                anchors.left: g.horizontalCenter
                anchors.right: g.right
            }

            Rectangle {
                id: n3
                color: "aquamarine"
                anchors.top: g.verticalCenter
                anchors.bottom: g.bottom
                anchors.left: g.left
                anchors.right: g.horizontalCenter
            }

            Rectangle {
                id: n4
                color: "lightgreen"
                anchors.top: g.verticalCenter
                anchors.bottom: g.bottom
                anchors.left: g.horizontalCenter
                anchors.right: g.right
            }
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
