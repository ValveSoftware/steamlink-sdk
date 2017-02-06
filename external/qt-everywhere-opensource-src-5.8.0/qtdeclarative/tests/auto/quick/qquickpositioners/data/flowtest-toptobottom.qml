import QtQuick 2.0

Item {
    height: 90
    width: 480
    property bool testRightToLeft: false

    Flow {
        objectName: "flow"
        height: parent.height
        layoutDirection: testRightToLeft ? Qt.RightToLeft : Qt.LeftToRight
        flow: Flow.TopToBottom
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
