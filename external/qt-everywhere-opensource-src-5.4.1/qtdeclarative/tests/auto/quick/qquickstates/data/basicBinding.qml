import QtQuick 2.0
Rectangle {
    id: myRectangle

    property color sourceColor: "blue"
    width: 100; height: 100
    color: "red"
    states: State {
        name: "blue"
        PropertyChanges { target: myRectangle; color: sourceColor }
    }
}
