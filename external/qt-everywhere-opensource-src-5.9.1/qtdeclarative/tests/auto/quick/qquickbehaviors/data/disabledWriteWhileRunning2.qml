import QtQuick 2.0
Rectangle {
    width: 400
    height: 400
    Rectangle {
        id: rect
        objectName: "MyRect"
        width: 100; height: 100; color: "green"
        Behavior on x {
            objectName: "MyBehavior"
            SmoothedAnimation { id: myAnimation; objectName: "MyAnimation"; duration: 1000; velocity: -1 }
        }
    }
}
