import QtQuick 1.1

Item {
    width: 640
    height: 480
    property bool testRightToLeft: true

    Grid {
        objectName: "grid"
        columns: 3
        layoutDirection: testRightToLeft ? Qt.RightToLeft : Qt.LeftToRight
        add: Transition {
            NumberAnimation {
                properties: "x,y";
            }
        }
        move: Transition {
            NumberAnimation {
                properties: "x,y";
            }
        }
        Rectangle {
            objectName: "one"
            color: "red"
            x: -100
            y: -100
            width: 50
            height: 50
        }
        Rectangle {
            objectName: "two"
            x: -100
            y: -100
            opacity: 0
            color: "green"
            width: 50
            height: 50
        }
        Rectangle {
            objectName: "three"
            color: "blue"
            x: -100
            y: -100
            width: 50
            height: 50
        }
        Rectangle {
            objectName: "four"
            color: "cyan"
            x: -100
            y: -100
            width: 50
            height: 50
        }
        Rectangle {
            objectName: "five"
            color: "magenta"
            x: -100
            y: -100
            width: 50
            height: 50
        }
    }
}
