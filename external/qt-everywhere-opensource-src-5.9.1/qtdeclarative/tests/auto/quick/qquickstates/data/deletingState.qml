import QtQuick 2.0
Rectangle {
    id: myRectangle
    width: 100; height: 100
    color: "red"
    StateGroup {
        id: stateGroup
        states: State {
            name: "blue"
            PropertyChanges { target: myRectangle; color: "blue" }
        }
    }
}
