import QtQuick 2.0
Rectangle {
    id: myRectangle
    width: 100; height: 100
    color: "red"
    states: [
        State {
            name: "blue"
            PropertyChanges { target: myRectangle; color: "blue" }
        },
        State {
            name: "bordered"
            extend: "blue"
            PropertyChanges { target: myRectangle; border.width: 2 }
        }]
}
