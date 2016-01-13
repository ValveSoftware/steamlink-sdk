import QtQuick 2.0

Item {
    id: root
    width: 320
    height: 480

    property real a: 200
    property real b: 10

    states: [
        State {
            name: "first"
            PropertyChanges { target: root; onBChanged: setA_1() }
        },
        State {
            name: "second"
            PropertyChanges { target: root; onBChanged: setA_2() }
        }
    ]

    function setA_1() { a = 50; }
    function setA_2() { a = 100; }
}
