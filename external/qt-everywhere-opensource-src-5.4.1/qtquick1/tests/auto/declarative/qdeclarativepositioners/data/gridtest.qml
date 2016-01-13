import QtQuick 1.1

Item {
    width: 640
    height: 480
    property bool testRightToLeft: false
    Grid {
        layoutDirection: testRightToLeft ? Qt.RightToLeft : Qt.LeftToRight
        objectName: "grid"
        columns: 3
        Rectangle {
            objectName: "one"
            color: "red"
            width: 50
            height: 50
        }
        Rectangle {
            objectName: "two"
            color: "green"
            width: 20
            height: 50
        }
        Rectangle {
            objectName: "three"
            color: "blue"
            width: 30
            height: 20
        }
        Rectangle {
            objectName: "four"
            color: "cyan"
            width: 50
            height: 50
        }
        Rectangle {
            objectName: "five"
            color: "magenta"
            width: 10
            height: 10
        }
    }
}
