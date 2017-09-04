import QtQuick 2.6

Item {
    width: 90
    height: 480
    property bool testRightToLeft: false

    Flow {
        objectName: "flow"
        width: parent.width
        layoutDirection: testRightToLeft ? Qt.RightToLeft : Qt.LeftToRight
        padding: 1; topPadding: 2; leftPadding: 3; rightPadding: 4; bottomPadding: 5
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
            width: 50
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
