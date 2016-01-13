import QtQuick 1.0

Rectangle {
    width: 240
    height: 320
    Rectangle {
        color: "red"
        ColorAnimation on color { from: 10; to: 15; }
        width: 50; height: 50
        x: 100; y: 100
    }
}
