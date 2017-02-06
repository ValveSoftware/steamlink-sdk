import QtQuick 2.0

Item {
    width: 640
    height: 480
    Column {
        objectName: "column"
        populate: Transition {
            NumberAnimation {
                properties: "y";
            }
        }
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
            visible: false
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
