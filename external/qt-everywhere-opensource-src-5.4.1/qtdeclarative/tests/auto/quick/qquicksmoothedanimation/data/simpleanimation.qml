import QtQuick 2.0

Rectangle {
    width: 300; height: 300;
    Rectangle {
        objectName: "rect"
        color: "red"
        width: 60; height: 60;
        x: 100; y: 100;
    }
    SmoothedAnimation { objectName: "anim"}
}