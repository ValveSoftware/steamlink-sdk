import QtQuick 2.0

Rectangle {
    id: root
    width: 400; height: 400
    property int someProp

    states: [
        State {
            name: "state1"
            PropertyChanges { target: root; onSomePropChanged: h1() }
        },
        State {
            name: "state2"
            PropertyChanges { target: root; onSomePropChanged: h2() }
        }
    ]

    // non-default handlers
    function h1() {}
    function h2() {}
}
