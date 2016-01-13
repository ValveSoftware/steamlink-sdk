import QtQuick 1.0

Rectangle {
    id: wrapper
    width: 240
    height: 320
    Rectangle {
        id: myRect
        color: "red"
        width: 50; height: 50
        x: 100; y: 100
    }
    states: State {
        name: "state1"
        PropertyChanges { target: myRect; border.color: "blue" }
    }
    transitions: Transition {
        ColorAnimation { target: myRect; to: "red"; property: "border.colr"; duration: 1000 }
    }
    Component.onCompleted: if (wrapper.state == "state1") wrapper.state = ""; else wrapper.state = "state1";
}
