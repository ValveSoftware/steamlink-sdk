import QtQuick 1.0

/*
Blue rect fills (with 10px margin) screen, then red, then green, then screen again.
*/

Rectangle {
    id: whiteRect
    width: 640; height: 480;

    Rectangle {
        id: redRect
        x: 400; y: 50
        width: 100; height: 100
        color: "red"
    }

    Rectangle {
        id: greenRect
        x: 100; y: 150
        width: 200; height: 300
        color: "green"
    }

    Rectangle {
        id: blueRect
        x: 5; y: 5
        width: parent.width-10
        height: parent.height-10
        color: "lightblue"

        //Text { text: "Click me!"; anchors.centerIn: parent }

        MouseArea {
            anchors.fill: parent
            onClicked: {
            switch(blueRect.state) {
                case "": blueRect.state = "inRed"; break;
                case "inRed": blueRect.state = "inGreen"; break;
                case "inGreen": blueRect.state = ""; break;
                }
            }
        }

        states: [
            State {
                name: "inRed"
                ParentChange { target: blueRect; parent: redRect; x: 5; y: 5; width: parent.width-10; height: parent.height-10 }
                PropertyChanges { target: redRect; z: 1 }
            },
            State {
                name: "inGreen"
                ParentChange { target: blueRect; parent: greenRect; x: 5; y: 5; width: parent.width-10; height: parent.height-10 }
                PropertyChanges { target: greenRect; z: 1 }
            }
        ]

        transitions: Transition {
            ParentAnimation { target: blueRect; //via: whiteRect;
                NumberAnimation { properties: "x, y, width, height"; duration: 500 }
            }
        }
    }
}
