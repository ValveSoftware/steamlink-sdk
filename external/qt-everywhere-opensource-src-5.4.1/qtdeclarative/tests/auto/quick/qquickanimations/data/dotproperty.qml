import QtQuick 2.0

Rectangle {
    id: wrapper
    width: 240
    height: 320
    Rectangle {
        id: myRect
        color: "red"
        width: 50; height: 50
        x: 100; y: 100
        MouseArea {
            anchors.fill: parent
            onClicked: if (wrapper.state == "state1") wrapper.state = ""; else wrapper.state = "state1";
        }
    }
    states: State {
        name: "state1"
        PropertyChanges { target: myRect; border.color: "blue" }
    }
    transitions: Transition {
        ColorAnimation { properties: "border.color"; duration: 1000 }
    }
}
