import QtQuick 2.0

// positioning done with bindings
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

        Rectangle {
            id: n1
            color: "green"
            width: c.width / 2
            height: c.height / 4
            x: 0
            y: c.height / 2
        }

        Rectangle {
            id: n2
            color: "cyan"
            width: c.width / 2
            height: c.height / 4
            x: c.width / 2
            y: c.height / 2
        }

        Rectangle {
            id: n3
            color: "aquamarine"
            width: c.width / 2
            height: c.height / 4
            x: 0
            y: (c.height / 4) * 3
        }

        Rectangle {
            id: n4
            color: "lightgreen"
            width: c.width / 2
            height: c.height / 4
            x: c.width / 2
            y: (c.height / 4) * 3
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
        //console.log("Binding Positioning: " + (t1.valueOf() - t0.valueOf()) + " milliseconds");
    }
}
