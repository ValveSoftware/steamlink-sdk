import QtQuick 1.0

Rectangle {
    id: myRect
    width: 400
    height: 400

    states: [
        State {
            name: "state1"
            PropertyChanges {
                target: myRect
                onHeightChanged: console.log("Hello World")
                color: "green"
            }
        },
        State {
            name: "state2"; extend: "state1"
            PropertyChanges {
                target: myRect
                color: "red"
        }
    }]
}
