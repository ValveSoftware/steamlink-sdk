import QtQuick 1.0

Rectangle {
    width: 400
    height: 400
    Rectangle {
        id: rect
        objectName: "MyRect"
        color: "red"
        width: 50; height: 50
        x: 100; y: 100
        NumberAnimation on x { id: anim; objectName: "MyAnim"; running: false; to: 200 }
    }
}
