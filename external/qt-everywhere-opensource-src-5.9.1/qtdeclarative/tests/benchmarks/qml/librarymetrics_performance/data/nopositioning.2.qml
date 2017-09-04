import QtQuick 2.0

// no bindings or anchors.
Rectangle {
    id: p
    color: "red"
    width: 400
    height: 800

    Rectangle {
        id: c
        color: "blue"
        width: 400
        height: 200

        Rectangle {
            id: n1
            color: "green"
            width: 200
            height: 50
        }

        Rectangle {
            id: n2
            color: "cyan"
            width: 200
            height: 50
        }

        Rectangle {
            id: n3
            color: "aquamarine"
            width: 200
            height: 50
        }

        Rectangle {
            id: n4
            color: "lightgreen"
            width: 200
            height: 50
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
        //console.log("No Positioning: " + (t1.valueOf() - t0.valueOf()) + " milliseconds");
    }
}
