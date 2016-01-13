import QtQuick 1.0

Rectangle {
    width: 200
    height: 200
    color: "blue"

    Rectangle {
        id: myRect
        objectName: "myRect"
        width: 100
        height: 100
        Behavior on x {
            NumberAnimation { duration: 500 }
        }
    }
}
