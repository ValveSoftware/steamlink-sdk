import QtQuick 2.0

// positioning via bindings with a grid
Rectangle {
    id: p
    color: "red"
    width: 400
    height: 800

    Rectangle {
        id: c
        color: "blue"
        width: p.width
        height: p.height / 2
        y: p.height / 2

        Grid {
            id: g
            rows: 2
            columns: 2
            width: c.width
            height: c.height / 2
            y: c.height / 2

            Rectangle {
                id: n1
                color: "green"
                width: g.width/2
                height: g.height/2
            }

            Rectangle {
                id: n2
                color: "cyan"
                width: g.width/2
                height: g.height/2
            }

            Rectangle {
                id: n3
                color: "aquamarine"
                width: g.width/2
                height: g.height/2
            }

            Rectangle {
                id: n4
                color: "lightgreen"
                width: g.width/2
                height: g.height/2
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
            // expand
            p.height = 800;
            // shrink
            p.height = 400;
        }
        //var t1 = new Date();
        //console.log("Binding With Grid Positioning: " + (t1.valueOf() - t0.valueOf()) + " milliseconds");
    }
}
