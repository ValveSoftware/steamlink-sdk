import QtQuick 1.0
Rectangle {
    id: myRectangle

    property color sourceColor: "red"
    width: 100; height: 100
    color: sourceColor
    states: State {
        name: "blue"
        PropertyChanges { target: myRectangle; color: "blue" }
    }
}
