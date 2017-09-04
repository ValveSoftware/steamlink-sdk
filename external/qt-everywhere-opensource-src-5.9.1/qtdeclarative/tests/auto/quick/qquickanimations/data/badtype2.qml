import QtQuick 2.0

Rectangle {
    width: 240
    height: 320
    Rectangle {
        color: "red"
        width: 50; height: 50
        x: 100; y: 100
        NumberAnimation on x { from: "blue"; to: "green"; }
    }
}
