import QtQuick 1.0

Rectangle {
    id: wrapper
    width: 240
    height: 320
    Rectangle {
        id: myRect
        objectName: "MyRect"
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
        PropertyChanges { target: myRect; x: 200; color: "blue" }
    }
    transitions: Transition {
        //comment out each in turn to make sure each only animates the relevant property
        ColorAnimation { properties: "x,color"; duration: 1000 } //x is real, color is color
        NumberAnimation { properties: "x,color"; duration: 1000 } //x is real, color is color
    }
}
