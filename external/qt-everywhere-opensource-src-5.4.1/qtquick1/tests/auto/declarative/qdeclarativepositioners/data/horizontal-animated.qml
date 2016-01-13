import QtQuick 1.1

Item {
    width: 640
    height: 480
    property bool testRightToLeft: false

    Row {
        objectName: "row"
        layoutDirection: testRightToLeft ? Qt.RightToLeft : Qt.LeftToRight
        add: Transition {
            NumberAnimation {
                properties: "x";
            }
        }
        move: Transition {
            NumberAnimation {
                properties: "x";
            }
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
            opacity: 0
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
