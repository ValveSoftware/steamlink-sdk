import QtQuick 2.0

// positioning all elements manually.
Rectangle {
    id: p
    color: "red"
    width: 400
    height: 800

    Rectangle {
        id: c
        color: "blue"
        width: 400
        height: 400
        y: 400

        Rectangle {
            id: n1
            color: "green"
            width: 200
            height: 100
            x: 0
            y: 200
        }

        Rectangle {
            id: n2
            color: "cyan"
            width: 200
            height: 100
            x: 200
            y: 200
        }

        Rectangle {
            id: n3
            color: "aquamarine"
            width: 200
            height: 100
            x: 0
            y: 300
        }

        Rectangle {
            id: n4
            color: "lightgreen"
            width: 200
            height: 100
            x: 200
            y: 300
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
    //            // expand
    //            p.height = 800
    //            c.height = 400
    //            c.y = 400
    //            n1.height = 100
    //            n1.y = 200
    //            n2.height = 100
    //            n2.y = 200
    //            n3.height = 100
    //            n3.y = 300
    //            n4.height = 100
    //            n4.y = 300
    //        } else {
    //            count = 0;
    //            // shrink
    //            p.height = 400
    //            c.height = 200
    //            c.y = 200
    //            n1.height = 50
    //            n1.y = 100
    //            n2.height = 50
    //            n2.y = 100
    //            n3.height = 50
    //            n3.y = 150
    //            n4.height = 50
    //            n4.y = 150
    //        }
    //    }
    //}

    Component.onCompleted: {
        var iterations = 10000;
        //var t0 = new Date();
        while (iterations > 0) {
            iterations--;
            // expand
            p.height = 800
            c.height = 400
            c.y = 400
            n1.height = 100
            n1.y = 200
            n2.height = 100
            n2.y = 200
            n3.height = 100
            n3.y = 300
            n4.height = 100
            n4.y = 300
            // shrink
            p.height = 400
            c.height = 200
            c.y = 200
            n1.height = 50
            n1.y = 100
            n2.height = 50
            n2.y = 100
            n3.height = 50
            n3.y = 150
            n4.height = 50
            n4.y = 150
        }
        //var t1 = new Date();
        //console.log("Absolute Positioning: " + (t1.valueOf() - t0.valueOf()) + " milliseconds");
    }
}
