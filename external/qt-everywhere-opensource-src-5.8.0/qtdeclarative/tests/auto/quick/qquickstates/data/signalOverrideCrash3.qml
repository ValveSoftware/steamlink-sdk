import QtQuick 2.0

Rectangle {
    id: myRect
    width: 400
    height: 400

    onHeightChanged:  console.log("base state")

    states: [
        State {
            name: "state1"
            PropertyChanges {
                target: myRect
                onHeightChanged: console.log("state1")
                color: "green"
            }
        },
        State {
            name: "state2";
            PropertyChanges {
                target: myRect
                onHeightChanged: console.log("state2")
                color: "red"
        }
    }]
}
