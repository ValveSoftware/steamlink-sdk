import QtQuick 2.0

Item {
    width: 640
    height: 480

    Row {
        objectName: "row"
        add: Transition {
            enabled: false
            NumberAnimation { properties: "x" }
        }
        move: Transition {
            enabled: false
            NumberAnimation { properties: "x" }
        }
        Rectangle {
            objectName: "one"
            color: "red"
            x: -100;
            width: 50
            height: 50
        }
        Rectangle {
            objectName: "two"
            color: "blue"
            x: -100;
            visible: false
            width: 50
            height: 50
        }
        Rectangle {
            objectName: "three"
            x: -100;
            color: "green"
            width: 50
            height: 50
        }
    }
}
