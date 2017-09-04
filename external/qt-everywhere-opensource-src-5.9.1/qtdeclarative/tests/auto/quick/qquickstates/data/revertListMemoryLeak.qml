import QtQuick 2.0

Item {
    id: rect
    width: 40;
    property real boundHeight: 42
    height: boundHeight;

    states: [
        State {
            objectName: "testState"
            name: "testState"
            PropertyChanges { target: rect; height: 60; }
        }
    ]
}
