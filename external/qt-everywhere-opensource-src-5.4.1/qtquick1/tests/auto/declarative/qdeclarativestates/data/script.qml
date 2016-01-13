import QtQuick 1.0
Rectangle {
    id: myRectangle
    width: 100; height: 100
    color: "red"
    states: State {
        name: "blue"
        StateChangeScript { script: myRectangle.color = "blue"; }
    }
}
