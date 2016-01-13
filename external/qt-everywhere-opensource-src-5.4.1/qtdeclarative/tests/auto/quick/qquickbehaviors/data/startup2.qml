import QtQuick 2.0

Rectangle {
    width: 800;
    height: 480;

    Text { id:theText; text: "hello world" }

    Rectangle {
        objectName: "innerRect"
        color: "red"
        x: theText.width
        Behavior on x { NumberAnimation {} }
        width: 100; height: 100
    }
}
