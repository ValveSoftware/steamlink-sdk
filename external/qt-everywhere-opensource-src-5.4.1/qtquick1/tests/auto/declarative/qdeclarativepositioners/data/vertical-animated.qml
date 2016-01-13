import QtQuick 1.0

Item {
    width: 640
    height: 480
    Column {
        objectName: "column"
        add: Transition {
            NumberAnimation {
                properties: "y";
            }
        }
        move: Transition {
            NumberAnimation {
                properties: "y";
            }
        }
        Rectangle {
            objectName: "one"
            color: "red"
            y: -100
            width: 50
            height: 50
        }
        Rectangle {
            objectName: "two"
            color: "blue"
            y: -100
            opacity: 0
            width: 50
            height: 50
        }
        Rectangle {
            objectName: "three"
            color: "red"
            y: -100
            width: 50
            height: 50
        }
    }
}
