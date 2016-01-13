import QtQuick 2.0

// positioning via anchors
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
            id: g
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
        var iterations = 10000;
        //var t0 = new Date();
        while (iterations > 0) {
            iterations--;
            p.height = 800;
            p.height = 400;
        }
        //var t1 = new Date();
        //console.log("Anchored Positioning: " + (t1.valueOf() - t0.valueOf()) + " milliseconds");
    }
}
